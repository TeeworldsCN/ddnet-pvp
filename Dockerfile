FROM alpine:3.4 AS builder
WORKDIR /ddnet-pvp
COPY . .
RUN apk update && apk upgrade && apk add g++ cmake make python3 sqlite-dev
RUN cmake -DCMAKE_BUILD_TYPE=Release . && make DDNet-Server -j

FROM alpine:3.4
WORKDIR /srv
COPY --from=builder /ddnet-pvp/DDNet-Server .
COPY --from=builder /ddnet-pvp/room_config room_config
COPY --from=builder /ddnet-pvp/autoexec.cfg .
COPY --from=builder /ddnet-pvp/data/maps/ maps
COPY --from=builder /ddnet-pvp/data/maps7/ maps7
COPY --from=builder /usr/lib/libgcc_s.so.1 /usr/lib/libstdc++.so.6 /usr/lib/libsqlite3.so.0 /usr/lib/
CMD ["./DDNet-Server"]
