// cp6: the WebGL side of the drawing layer, plus the WebSocket to the server.
// C++ builds a frame as an array of instances (one quad per shape) and a path point buffer,
// then hands both over in one cp6_flush call.

addToLibrary({
  $CP6: {
    gl: null, canvas: null, prog: null, vao: null, ibuf: null,
    pathTex: null, pathRows: 0, atlasTex: null, atlasCtx: null, glyphCtx: null,
    shelfX: 0, shelfY: 0, shelfH: 0, ATLAS: 2048,
    images: [], inbox: [],
  },

  cp6_init__deps: ['$CP6'],
  cp6_init: function () {
    const canvas = document.getElementById('canvas');
    const gl = canvas.getContext('webgl2', { alpha: false, antialias: false, depth: false, stencil: false, premultipliedAlpha: true, preserveDrawingBuffer: false });
    if (!gl) return 0;
    CP6.canvas = canvas; CP6.gl = gl;
    const vs = `#version 300 es
layout(location=0) in vec4 a0; layout(location=1) in vec4 a1; layout(location=2) in vec4 a2;
layout(location=3) in vec4 a3; layout(location=4) in vec4 a4; layout(location=5) in vec4 aFill; layout(location=6) in vec4 aStroke;
uniform vec2 uView;
out vec2 vLocal; out vec2 vDev;
flat out vec4 fKind; flat out vec4 fP; flat out vec4 fQ; flat out vec4 fFill; flat out vec4 fStroke; flat out vec4 fBox;
void main() {
  vec2 corner = vec2((gl_VertexID & 1) == 0 ? -1.0 : 1.0, (gl_VertexID & 2) == 0 ? -1.0 : 1.0);
  float scale = sqrt(max(abs(a0.z * a1.y - a0.w * a1.x), 1e-12));
  float px = 1.0 / scale;
  float kind = a2.z;
  float pad = kind > 3.5 && kind < 5.5 ? 0.0 : a2.w * 0.5 + 1.5 * px;
  if (kind > 5.5 && kind < 6.5) pad = a2.w * 0.5 + 1.5 * px;
  vec2 local = a1.zw + corner * (a2.xy + pad);
  vec2 dev = a0.xy + a0.zw * local.x + a1.xy * local.y;
  vLocal = local; vDev = dev;
  fKind = vec4(kind, a2.w, px, a4.y);
  fP = a3; fQ = vec4(a4.x, a4.z, a4.w, 0.0);
  fBox = vec4(a1.zw, a2.xy);
  fFill = aFill; fStroke = aStroke;
  gl_Position = vec4(dev.x / uView.x * 2.0 - 1.0, 1.0 - dev.y / uView.y * 2.0, 0.0, 1.0);
}`;
    const fs = `#version 300 es
precision highp float; precision highp int;
in vec2 vLocal; in vec2 vDev;
flat in vec4 fKind; flat in vec4 fP; flat in vec4 fQ; flat in vec4 fFill; flat in vec4 fStroke; flat in vec4 fBox;
uniform highp sampler2D uPath; uniform sampler2D uAtlas; uniform sampler2D uImage;
out vec4 o;
vec2 P(int i) { return texelFetch(uPath, ivec2(i & 1023, i >> 10), 0).xy; }
float cr(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }
int wn(vec2 p, vec2 a, vec2 b) {
  if (a.y <= p.y) { if (b.y > p.y && cr(b - a, p - a) > 0.0) return 1; }
  else if (b.y <= p.y && cr(b - a, p - a) < 0.0) return -1;
  return 0;
}
// cap: 0 butt, 1 round, 2 square; s/e: segment starts/ends an open subpath
float segd(vec2 p, vec2 a, vec2 b, int cap, bool s, bool e, float hw) {
  vec2 ba = b - a; float l2 = max(dot(ba, ba), 1e-12);
  if (cap != 1 && (s || e)) {
    vec2 dir = ba * inversesqrt(l2);
    if (cap == 2) { if (s) a -= dir * hw; if (e) b += dir * hw; ba = b - a; l2 = max(dot(ba, ba), 1e-12); }
    float t = dot(p - a, ba) / l2;
    if ((s && t < 0.0) || (e && t > 1.0)) return 1e20;
  }
  float h = clamp(dot(p - a, ba) / l2, 0.0, 1.0);
  return length(p - a - ba * h);
}
// returns (signed fill distance, stroke distance)
vec2 pathSD(vec2 p, int off, int cnt, int rule, int cap, float hw) {
  float dF = 1e20, dS = 1e20; int w = 0; int i = off; int end = off + cnt;
  while (i < end) {
    vec2 hd = P(i); int n = int(hd.x); bool closed = hd.y > 0.5; i++;
    vec2 first = P(i); vec2 prev = first;
    for (int k = 1; k < n; k++) {
      vec2 cur = P(i + k);
      vec2 ba = cur - prev; float h = clamp(dot(p - prev, ba) / max(dot(ba, ba), 1e-12), 0.0, 1.0);
      float d = length(p - prev - ba * h);
      dF = min(dF, d);
      dS = min(dS, closed ? d : segd(p, prev, cur, cap, k == 1, k == n - 1, hw));
      w += wn(p, prev, cur);
      prev = cur;
    }
    if (n > 1) {
      vec2 ba = first - prev; float h = clamp(dot(p - prev, ba) / max(dot(ba, ba), 1e-12), 0.0, 1.0);
      float d = length(p - prev - ba * h);
      dF = min(dF, d); if (closed) dS = min(dS, d);
      w += wn(p, prev, first);
    } else if (cap != 0) {
      dS = min(dS, length(p - first));
    }
    i += n;
  }
  bool inside = rule == 0 ? w != 0 : (w & 1) != 0;
  return vec2(inside ? -dF : dF, dS);
}
void main() {
  int kind = int(fKind.x + 0.5); float sw = fKind.y; float px = fKind.z; float alpha = fKind.w;
  float hw = sw * 0.5;
  vec2 p = vLocal; vec2 c = fBox.xy; vec2 h = fBox.zw;
  float dF = 1e20, dS = 1e20; vec4 img = vec4(0.0); bool glyph = false;
  if (kind == 0) { // rect
    vec2 q = abs(p - c) - h; dF = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0); dS = abs(dF);
  } else if (kind == 1) { // ellipse (exact for circles)
    vec2 q = p - c;
    if (abs(h.x - h.y) < 1e-4 * h.x) dF = length(q) - h.x;
    else { float k1 = length(q / h); float k2 = length(q / (h * h)); dF = k1 * (k1 - 1.0) / max(k2, 1e-12); }
    dS = abs(dF);
  } else if (kind == 2) { // path
    vec2 r = pathSD(p, int(fP.x), int(fP.y), int(fP.z), int(fP.w), hw); dF = r.x; dS = r.y;
  } else if (kind == 3) { // segment list is a path too; reserved
  } else if (kind == 4) { // glyph
    vec2 t = (p - (c - h)) / (2.0 * h);
    vec2 uv = mix(fP.xy, fP.zw, t);
    float v = texture(uAtlas, uv / 2048.0).r;
    dF = (0.5 - v) * fQ.x - 0.3 * px; dS = abs(dF); glyph = true; // a little heavier, like browser text at small sizes
  } else if (kind == 5) { // image
    vec2 t = (p - (c - h)) / (2.0 * h);
    img = texture(uImage, mix(fP.xy, fP.zw, t));
  } else if (kind == 6) { // grid lines: fP = (spacing, ox, oy, 0)
    vec2 g = abs(mod(p - fP.yz + fP.x * 0.5, fP.x) - fP.x * 0.5);
    dS = min(g.x, g.y);
  }
  vec4 col;
  if (kind == 5) {
    vec2 q = abs(p - c) - h; float d = max(q.x, q.y);
    col = img * clamp(0.5 - d / px, 0.0, 1.0);
  } else {
    float fa = fFill.a > 0.0 ? clamp(0.5 - dF / px, 0.0, 1.0) * fFill.a : 0.0;
    float sa = 0.0;
    if (sw > 0.0 && fStroke.a > 0.0) sa = clamp((min(dS + px * 0.5, hw) - max(dS - px * 0.5, -hw)) / px, 0.0, 1.0) * fStroke.a;
    vec4 F = vec4(fFill.rgb * fa, fa), S = vec4(fStroke.rgb * sa, sa);
    col = glyph ? F + S * (1.0 - fa) : S + F * (1.0 - sa);
  }
  int nc = int(fQ.z + 0.5);
  for (int k = 0; k < nc; k++) {
    vec2 oc = P(int(fQ.y) + k);
    float d;
    if (oc.y < -0.5) { // exact circle: (cx, cy), (r, 0)
      d = length(vDev - P(int(oc.x))) - P(int(oc.x) + 1).x;
      if (oc.y < -1.5) d = -d;
    } else d = pathSD(vDev, int(oc.x), int(oc.y), 0, 1, 0.0).x;
    col *= clamp(0.5 - d, 0.0, 1.0);
  }
  o = col * alpha;
}`;
    const sh = (t, s) => { const x = gl.createShader(t); gl.shaderSource(x, s); gl.compileShader(x); if (!gl.getShaderParameter(x, gl.COMPILE_STATUS)) console.error(gl.getShaderInfoLog(x)); return x; };
    const prog = gl.createProgram();
    gl.attachShader(prog, sh(gl.VERTEX_SHADER, vs)); gl.attachShader(prog, sh(gl.FRAGMENT_SHADER, fs));
    gl.linkProgram(prog);
    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) console.error(gl.getProgramInfoLog(prog));
    CP6.prog = prog;
    CP6.uView = gl.getUniformLocation(prog, 'uView');
    gl.useProgram(prog);
    gl.uniform1i(gl.getUniformLocation(prog, 'uPath'), 0);
    gl.uniform1i(gl.getUniformLocation(prog, 'uAtlas'), 1);
    gl.uniform1i(gl.getUniformLocation(prog, 'uImage'), 2);
    CP6.vao = gl.createVertexArray(); gl.bindVertexArray(CP6.vao);
    CP6.ibuf = gl.createBuffer(); gl.bindBuffer(gl.ARRAY_BUFFER, CP6.ibuf);
    for (let i = 0; i < 7; i++) { gl.enableVertexAttribArray(i); gl.vertexAttribDivisor(i, 1); }
    CP6.pathTex = gl.createTexture();
    gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, CP6.pathTex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    CP6.atlasTex = gl.createTexture();
    gl.activeTexture(gl.TEXTURE1); gl.bindTexture(gl.TEXTURE_2D, CP6.atlasTex);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.R8, CP6.ATLAS, CP6.ATLAS, 0, gl.RED, gl.UNSIGNED_BYTE, null);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    CP6.blank = gl.createTexture();
    gl.activeTexture(gl.TEXTURE2); gl.bindTexture(gl.TEXTURE_2D, CP6.blank);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, 1, 1, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array(4));
    gl.enable(gl.BLEND); gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
    const gc = document.createElement('canvas'); gc.width = 160; gc.height = 160;
    CP6.glyphCtx = gc.getContext('2d', { willReadFrequently: true });
    CP6.fontReady = !document.fonts;
    if (document.fonts) document.fonts.load('700 48px Ubuntu').then(() => { CP6.fontReady = true; }, () => { CP6.fontReady = true; });
    setTimeout(() => { CP6.fontReady = true; }, 4000);
    return 1;
  },

  // Sizes the canvas to the window in device pixels; writes w, h (int) and dpr (float).
  cp6_size__deps: ['$CP6'],
  cp6_size: function (out) {
    const dpr = window.devicePixelRatio || 1;
    const w = Math.round(window.innerWidth * dpr), h = Math.round(window.innerHeight * dpr);
    const c = CP6.canvas;
    if (c.width !== w || c.height !== h) { c.width = w; c.height = h; }
    HEAP32[out >> 2] = w; HEAP32[(out >> 2) + 1] = h; HEAPF32[(out >> 2) + 2] = dpr;
  },

  // inst: count instances of 88 bytes; path: texels RG32F; batches: (start, count, image) int triples
  cp6_flush__deps: ['$CP6'],
  cp6_flush: function (inst, count, path, texels, batches, nbatch, clear) {
    const gl = CP6.gl; if (!gl) return;
    const W = CP6.canvas.width, H = CP6.canvas.height;
    gl.bindFramebuffer(gl.FRAMEBUFFER, null);
    gl.viewport(0, 0, W, H);
    gl.clearColor(((clear >> 16) & 255) / 255, ((clear >> 8) & 255) / 255, (clear & 255) / 255, 1);
    gl.clear(gl.COLOR_BUFFER_BIT);
    if (!count) return;
    gl.useProgram(CP6.prog); gl.bindVertexArray(CP6.vao);
    gl.uniform2f(CP6.uView, W, H);
    gl.activeTexture(gl.TEXTURE0); gl.bindTexture(gl.TEXTURE_2D, CP6.pathTex);
    const rows = Math.max(1, Math.ceil(texels / 1024));
    if (rows > CP6.pathRows) {
      CP6.pathRows = Math.max(rows, CP6.pathRows * 2, 16);
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RG32F, 1024, CP6.pathRows, 0, gl.RG, gl.FLOAT, null);
    }
    if (texels) gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, 1024, rows, gl.RG, gl.FLOAT, HEAPF32.subarray(path >> 2, (path >> 2) + rows * 2048));
    gl.activeTexture(gl.TEXTURE1); gl.bindTexture(gl.TEXTURE_2D, CP6.atlasTex);
    gl.bindBuffer(gl.ARRAY_BUFFER, CP6.ibuf);
    gl.bufferData(gl.ARRAY_BUFFER, HEAPU8.subarray(inst, inst + count * 88), gl.STREAM_DRAW);
    for (let b = 0; b < nbatch; b++) {
      const start = HEAP32[(batches >> 2) + b * 3], n = HEAP32[(batches >> 2) + b * 3 + 1], img = HEAP32[(batches >> 2) + b * 3 + 2];
      if (img === -1) {  // draw into the layer, starting transparent
        if (!CP6.layerFbo || CP6.layerW !== W || CP6.layerH !== H) {
          if (!CP6.layerFbo) { CP6.layerFbo = gl.createFramebuffer(); CP6.layerTex = gl.createTexture(); }
          gl.activeTexture(gl.TEXTURE2); gl.bindTexture(gl.TEXTURE_2D, CP6.layerTex);
          gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, W, H, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
          gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE); gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
          gl.bindFramebuffer(gl.FRAMEBUFFER, CP6.layerFbo);
          gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, CP6.layerTex, 0);
          CP6.layerW = W; CP6.layerH = H;
        }
        gl.bindFramebuffer(gl.FRAMEBUFFER, CP6.layerFbo);
        gl.clearColor(0, 0, 0, 0); gl.clear(gl.COLOR_BUFFER_BIT);
        continue;
      }
      if (img === -2) { gl.bindFramebuffer(gl.FRAMEBUFFER, null); continue; }
      if (!n) continue;
      const base = start * 88;
      for (let i = 0; i < 5; i++) gl.vertexAttribPointer(i, 4, gl.FLOAT, false, 88, base + i * 16);
      gl.vertexAttribPointer(5, 4, gl.UNSIGNED_BYTE, true, 88, base + 80);
      gl.vertexAttribPointer(6, 4, gl.UNSIGNED_BYTE, true, 88, base + 84);
      gl.activeTexture(gl.TEXTURE2);
      gl.bindTexture(gl.TEXTURE_2D, img === -3 ? CP6.layerTex : img > 0 && CP6.images[img] ? CP6.images[img] : CP6.blank);
      gl.drawArraysInstanced(gl.TRIANGLE_STRIP, 0, 4, n);
    }
  },

  // Rasterizes one glyph of Ubuntu 700 at 48px into the SDF atlas.
  // out (floats): advance, x0, y0, x1, y1 (box relative to the pen at the baseline, base px), u0, v0, u1, v1
  cp6_glyph__deps: ['$CP6'],
  cp6_glyph: function (cp, out) {
    const SIZE = 48, PAD = 12, g = CP6.glyphCtx, gl = CP6.gl;
    const ch = String.fromCodePoint(cp);
    g.font = '700 ' + SIZE + 'px Ubuntu';
    const m = g.measureText(ch);
    const f = HEAPF32, o = out >> 2;
    f[o] = m.width;
    const l = Math.floor(-m.actualBoundingBoxLeft), r = Math.ceil(m.actualBoundingBoxRight);
    const t = Math.ceil(m.actualBoundingBoxAscent), b = Math.ceil(m.actualBoundingBoxDescent);
    const gw = Math.max(0, r - l), gh = Math.max(0, t + b);
    if (!gw || !gh || ch.trim() === '') { for (let i = 1; i < 9; i++) f[o + i] = 0; return; }
    const w = gw + 2 * PAD, h = gh + 2 * PAD;
    if (w > g.canvas.width || h > g.canvas.height) { g.canvas.width = Math.max(w, g.canvas.width); g.canvas.height = Math.max(h, g.canvas.height); g.font = '700 ' + SIZE + 'px Ubuntu'; }
    g.clearRect(0, 0, w, h);
    g.fillStyle = '#fff'; g.textBaseline = 'alphabetic'; g.textAlign = 'left';
    g.fillText(ch, PAD - l, PAD + t);
    const data = g.getImageData(0, 0, w, h).data;
    // Felzenszwalb EDT on outside and inside distances (TinySDF approach)
    const INF = 1e20, n = w * h;
    const outer = new Float64Array(n), inner = new Float64Array(n);
    for (let i = 0; i < n; i++) {
      const a = data[i * 4 + 3] / 255;
      if (a === 1) { outer[i] = 0; inner[i] = INF; }
      else if (a === 0) { outer[i] = INF; inner[i] = 0; }
      else { const d = 0.5 - a; outer[i] = d > 0 ? d * d : 0; inner[i] = d < 0 ? d * d : 0; }
    }
    const size = Math.max(w, h);
    const ff = new Float64Array(size), zz = new Float64Array(size + 1), vv = new Uint16Array(size), dd = new Float64Array(size);
    const edt1 = (grid, offset, stride, len) => {
      for (let q = 0; q < len; q++) ff[q] = grid[offset + q * stride];
      vv[0] = 0; zz[0] = -INF; zz[1] = INF; let k = 0;
      for (let q = 1; q < len; q++) {
        let s;
        do { const r = vv[k]; s = (ff[q] - ff[r] + q * q - r * r) / (q - r) / 2; } while (s <= zz[k] && --k > -1);
        k++; vv[k] = q; zz[k] = s; zz[k + 1] = INF;
      }
      for (let q = 0, k2 = 0; q < len; q++) { while (zz[k2 + 1] < q) k2++; const r = vv[k2]; grid[offset + q * stride] = ff[r] + (q - r) * (q - r); }
    };
    const edt = grid => { for (let x = 0; x < w; x++) edt1(grid, x, w, h); for (let y = 0; y < h; y++) edt1(grid, y * w, 1, w); };
    edt(outer); edt(inner);
    const px = new Uint8Array(n);
    for (let i = 0; i < n; i++) {
      const d = Math.sqrt(outer[i]) - Math.sqrt(inner[i]);
      px[i] = Math.max(0, Math.min(255, Math.round(255 * (0.5 - d / (2 * PAD)))));
    }
    if (CP6.shelfX + w > CP6.ATLAS) { CP6.shelfX = 0; CP6.shelfY += CP6.shelfH + 1; CP6.shelfH = 0; }
    if (CP6.shelfY + h > CP6.ATLAS) { CP6.shelfX = 0; CP6.shelfY = 0; CP6.shelfH = 0; }
    const ax = CP6.shelfX, ay = CP6.shelfY;
    CP6.shelfX += w + 1; CP6.shelfH = Math.max(CP6.shelfH, h);
    gl.activeTexture(gl.TEXTURE1); gl.bindTexture(gl.TEXTURE_2D, CP6.atlasTex);
    gl.texSubImage2D(gl.TEXTURE_2D, 0, ax, ay, w, h, gl.RED, gl.UNSIGNED_BYTE, px);
    f[o + 1] = l - PAD; f[o + 2] = -t - PAD; f[o + 3] = r + PAD; f[o + 4] = b + PAD;
    f[o + 5] = ax; f[o + 6] = ay; f[o + 7] = ax + w; f[o + 8] = ay + h;
  },

  cp6_font_ready__deps: ['$CP6'],
  cp6_font_ready: function () { return CP6.fontReady ? 1 : 0; },

  cp6_now: function () { return performance.now(); },

  // wheel notches since the last call; a trackpad's small steps count three times, lines and
  // pages far less
  ui_wheel__deps: ['$CP6'],
  ui_wheel: function () {
    if (!CP6.wheelOn) {
      CP6.wheelOn = true; CP6.wheel = 0;
      window.addEventListener('wheel', e => {
        const pad = e.wheelDeltaY ? e.wheelDeltaY === -3 * e.deltaY : e.deltaMode === 0;
        let d = e.deltaY / 175;
        if (pad) d *= 3;
        if (e.deltaMode === 1) d /= 18; else if (e.deltaMode === 2) d /= 1000;
        CP6.wheel += d;
      });
    }
    const w = CP6.wheel; CP6.wheel = 0;
    return w;
  },

  // a WebSocket back to the server that served the page; replies wait in the inbox
  net_open__deps: ['$CP6'],
  net_open: function () {
    CP6.inbox = [];
    if (CP6.ws) { CP6.ws.onmessage = null; try { CP6.ws.close(); } catch (e) {} }
    const ws = new WebSocket((location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/');
    ws.binaryType = 'arraybuffer';
    ws.onmessage = e => { if (e.data.byteLength) CP6.inbox.push(new Uint8Array(e.data)); };
    CP6.ws = ws;
  },
  net_send__deps: ['$CP6'],
  net_send: function (data, len) {
    if (CP6.ws && CP6.ws.readyState === 1) CP6.ws.send(HEAPU8.slice(data, data + len));
  },
  // 0 connecting, 1 open, 2 closed
  net_state__deps: ['$CP6'],
  net_state: function () { const s = CP6.ws ? CP6.ws.readyState : 3; return s === 0 ? 0 : s === 1 ? 1 : 2; },
  // copies the next reply into buf; returns its size, -1 if none, or -size if cap is too small
  net_next__deps: ['$CP6'],
  net_next: function (buf, cap) {
    if (!CP6.inbox.length) return -1;
    const m = CP6.inbox[0];
    if (m.length > cap) return -m.length;
    HEAPU8.set(m, buf); CP6.inbox.shift();
    return m.length;
  },

  ui_user_agent: function (buf, cap) { return stringToUTF8(navigator.userAgent, buf, cap); },
  ui_language: function (buf, cap) { return stringToUTF8(navigator.language || 'en-US', buf, cap); },
  ui_storage_get: function (key, buf, cap) {
    try { const v = localStorage.getItem(UTF8ToString(key)); if (v == null) return -1; return stringToUTF8(v, buf, cap); } catch (e) { return -1; }
  },
  ui_storage_set: function (key, val) { try { localStorage.setItem(UTF8ToString(key), UTF8ToString(val)); } catch (e) {} },
  ui_cursor: function (kind) { CP6.canvas.style.cursor = ['default', 'pointer', 'text'][kind] || 'default'; },
  ui_open: function (url) { try { window.open(UTF8ToString(url), '_blank').focus(); } catch (e) {} },
  // 1: a phone or tablet, 2: a tablet, going by the user agent
  ui_device: function () {
    const ua = navigator.userAgent;
    if (!/iPhone|iPad|Android|Mobile|Tablet/.test(ua)) return 0;
    return /iPad|Tablet/.test(ua) ? 2 : 1;
  },
  // asks for a name with the browser's own dialog; -1 when cancelled
  ui_prompt: function (cur, buf, cap) {
    let r = null;
    try { r = window.prompt(undefined, UTF8ToString(cur)); } catch (e) {}
    return r == null ? -1 : stringToUTF8(r, buf, cap);
  },
});
