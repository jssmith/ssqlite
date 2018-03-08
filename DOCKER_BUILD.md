Instructions for Building with Docker
=====================================

Docker is useful for reproducible builds, as well as for building Linux binaries
on OS X or other platforms.

```
docker build docker-dev -t ssqlite-dev:latest
```

```
docker run -t -i -v $(pwd):/ssqlite ssqlite-dev
```

In the Docker instance
```
cd /ssqlite/nfsv4
make
```
