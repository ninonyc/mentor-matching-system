FROM debian:bookworm-slim AS builder
RUN apt-get update && apt-get install -y --no-install-recommends g++ gcc pkg-config libssl-dev ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY algorithm/ algorithm/
COPY backend/ backend/
RUN bash backend/build.sh

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends libssl3 libstdc++6 python3 caddy ca-certificates && rm -rf /var/lib/apt/lists/* \
    && groupadd --gid 10001 ascent && useradd --uid 10001 --gid ascent --create-home ascent
WORKDIR /app
COPY --from=builder /app/build/mentor_api /app/build/mentor_api
COPY database/migrations/ /app/database/migrations/
COPY frontend/ /app/frontend/
COPY deployment/ /app/deployment/
ENV PORT=10000 MENTOR_DATA_DIR=/var/data
# Validate routing syntax during the container build.
RUN caddy validate --config /app/deployment/Caddyfile --adapter caddyfile
EXPOSE 10000
# The supervisor prepares volume permissions, then drops to UID/GID 10001.
CMD ["python3", "/app/deployment/start.py"]
