# syntax=docker/dockerfile:1

FROM emscripten/emsdk:6.0.0 AS client
RUN apt-get update \
 && apt-get install -y --no-install-recommends cmake ninja-build \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src/client
COPY shared /src/shared
COPY client/CMakeLists.txt ./
COPY client/web web
COPY client/src src
RUN emcmake cmake -S . -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release \
 && ninja -C /build client

FROM alpine:3.24.4 AS server
RUN apk add --no-cache build-base cmake ninja git linux-headers zlib-dev zlib-static
WORKDIR /src/server
# Keep in step with the server/third_party/uWebSockets submodule.
ARG UWS_COMMIT=8a1f3607f4d6cf3249fa6430543fc4134645c7b1
RUN git init -q third_party/uWebSockets \
 && cd third_party/uWebSockets \
 && git remote add origin https://github.com/uNetworking/uWebSockets \
 && git fetch -q --depth 1 origin "$UWS_COMMIT" \
 && git checkout -q FETCH_HEAD \
 && git submodule update -q --init --depth 1 uSockets
COPY shared /src/shared
COPY server/CMakeLists.txt ./
COPY server/src src
RUN cmake -S . -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS=-static \
 && ninja -C /build server \
 && strip /build/server

FROM scratch
WORKDIR /app/server
COPY --from=server /build/server ./server
COPY --from=client /src/client/dist /app/client/dist
ENV STATIC=/app/client/dist
USER 65532:65532
EXPOSE 8098
ENTRYPOINT ["/app/server/server"]
