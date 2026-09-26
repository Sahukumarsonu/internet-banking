# ---- Build stage -----------------------------------------------------
FROM debian:bookworm-slim AS build


RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    libboost-system-dev \
    libboost-dev \
    libssl-dev \
    libsqlite3-dev \
    libasio-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Crow is header-only; fetch the single-header release into /usr/local/include.
RUN mkdir -p /usr/local/include && \
    git clone --depth 1 --branch v1.2.0 https://github.com/CrowCpp/Crow.git /tmp/crow && \
    cp -r /tmp/crow/include/* /usr/local/include/ && \
    rm -rf /tmp/crow

WORKDIR /app
COPY backend/ /app/backend/
COPY database/ /app/database/

WORKDIR /app/backend
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release -j"$(nproc)"

# ---- Runtime stage -----------------------------------------------------
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl3 \
    libsqlite3-0 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/backend/build/bank_server /app/bank_server
COPY --from=build /app/backend/build/database /app/database
COPY database/ /app/database/

# Render sets $PORT at runtime; the app reads it via config.h.
ENV BANK_DB_PATH=/data/bank.db
EXPOSE 8080

# Render provides a persistent disk mounted at /data if you configure one
# in the dashboard (Settings -> Disks). Without it, the SQLite file is
# recreated on every deploy/restart and all data is lost — see README.
RUN mkdir -p /data

CMD ["/app/bank_server"]
