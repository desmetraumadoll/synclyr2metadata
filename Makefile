# ── synclyr2metadata Makefile ──────────────────────────────────────────
#
#   make            → Static build via Docker (zero dependencies, recommended)
#   make native     → Local dynamic build (needs libcurl-dev, taglib-dev)
#   make install    → Install to /usr/local/bin
#   make clean      → Remove build artifacts
#

CC       = gcc
CFLAGS   = -O2 -Wall -Wextra -pedantic -std=c11 -D_POSIX_C_SOURCE=200809L
TARGET   = synclyr2metadata

SRC_DIR   = src
INC_DIR   = include
THIRD_DIR = third_party/cjson
BUILD_DIR = build

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/http_client.c \
       $(SRC_DIR)/lidarr.c \
       $(SRC_DIR)/lrclib.c \
       $(SRC_DIR)/metadata.c \
       $(SRC_DIR)/sync.c \
       $(THIRD_DIR)/cJSON.c

OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o)
LDFLAGS = -lcurl -ltag_c -ltag -lz -lpthread

PREFIX ?= /usr/local

# ── Targets ────────────────────────────────────────────────────────────

.PHONY: all native check-native-deps clean debug install uninstall

# Default: static build via Docker (self-contained, works everywhere)
all:
	docker run --rm -v "$$(pwd)":/build -w /build alpine:3.22 sh -c " \
		apk add build-base cmake git pkgconf curl-dev curl-static \
			openssl-libs-static zlib-static brotli-static nghttp2-static \
			zstd-static libidn2-static libunistring-static libpsl-static \
			c-ares-dev && \
		cd /tmp && \
		git clone --depth 1 --branch v2.0.2 https://github.com/taglib/taglib.git && \
		cd taglib && git submodule update --init && \
		cmake -B build -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_INSTALL_PREFIX=/usr -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF && \
		cmake --build build -j\$$(nproc) && \
		cmake --install build && \
		cd /build && \
		make native \
			LDFLAGS='-static \
				/usr/lib/libtag_c.a /usr/lib/libtag.a \
				-lcurl -lssl -lcrypto -lz -lbrotlidec -lbrotlicommon \
				-lzstd -lnghttp2 -lpsl -lidn2 -lunistring -lcares \
				-lstdc++ -lm -lpthread' && \
		strip /build/synclyr2metadata"

# Local dynamic build (requires libcurl-dev + taglib-dev on your system)
native: check-native-deps clean $(TARGET)

check-native-deps:
	@printf '%s\n' 'Checking native build dependencies...'
	@printf '%s\n' '#include <curl/curl.h>' | $(CC) -x c - -c -o /tmp/synclyr2metadata-curl-check.o >/dev/null 2>&1 || { \
		printf '%s\n' 'error: missing libcurl development headers (curl/curl.h).'; \
		printf '%s\n' 'Debian/Ubuntu: sudo apt install build-essential libcurl4-openssl-dev libtag1-dev'; \
		printf '%s\n' 'Arch:          sudo pacman -S base-devel curl taglib'; \
		exit 1; \
	}
	@rm -f /tmp/synclyr2metadata-curl-check.o
	@printf '%s\n' '#include <taglib/tag_c.h>' | $(CC) -x c - -c -o /tmp/synclyr2metadata-taglib-check.o >/dev/null 2>&1 || { \
		printf '%s\n' 'error: missing TagLib C development headers (taglib/tag_c.h).'; \
		printf '%s\n' 'Debian/Ubuntu: sudo apt install build-essential libcurl4-openssl-dev libtag1-dev'; \
		printf '%s\n' 'Arch:          sudo pacman -S base-devel curl taglib'; \
		exit 1; \
	}
	@rm -f /tmp/synclyr2metadata-taglib-check.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I$(THIRD_DIR) -c $< -o $@

debug: CFLAGS := -g -O0 -Wall -Wextra -pedantic -std=c11 -fsanitize=address,undefined
debug: LDFLAGS += -fsanitize=address,undefined
debug: clean $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
