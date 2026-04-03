FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    g++ \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY main.cpp client.cpp client.h ./

RUN g++ main.cpp client.cpp -o worker_node -O3 -pthread -lcrypto

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/worker_node .

ENTRYPOINT ["./worker_node"]