#include "relay_state_store.h"
#include <string.h>

// Simple CRC32 (poly 0xEDB88320) implementation
static uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

size_t encodeRelayState(const PersistedRelayBlock& block, uint32_t seq, uint8_t* outBuf, size_t maxLen, uint32_t& outCrc) {
    // Layout: magic(4) version(1) seq(4) block(sizeof) crc32(4) = tail
    const uint32_t MAGIC = 0x52534C59; // 'RSLY'
    const uint8_t VERSION = 1;
    size_t need = 4 + 1 + 4 + sizeof(PersistedRelayBlock) + 4;
    if (maxLen < need) return 0;
    uint8_t* p = outBuf;
    memcpy(p, &MAGIC, 4); p += 4;
    *p++ = VERSION;
    memcpy(p, &seq, 4); p += 4;
    memcpy(p, &block, sizeof(block)); p += sizeof(block);
    outCrc = crc32_update(0, outBuf, 4 + 1 + 4 + sizeof(block));
    memcpy(p, &outCrc, 4); p += 4;
    return need;
}

bool decodeRelayState(const uint8_t* data, size_t len, PersistedRelayBlock& block, uint32_t& seq) {
    if (len < 4 + 1 + 4 + sizeof(PersistedRelayBlock) + 4) return false;
    uint32_t magic; memcpy(&magic, data, 4);
    if (magic != 0x52534C59) return false;
    uint8_t version = data[4];
    if (version != 1) return false; // incompatible
    memcpy(&seq, data + 5, 4);
    memcpy(&block, data + 9, sizeof(PersistedRelayBlock));
    uint32_t storedCrc; memcpy(&storedCrc, data + 9 + sizeof(PersistedRelayBlock), 4);
    uint32_t calc = crc32_update(0, data, 4 + 1 + 4 + sizeof(PersistedRelayBlock));
    return storedCrc == calc;
}

#ifdef ARDUINO
#include <LittleFS.h>
#include "fs_utils.h"

static const char* SLOT_A = "/relay_state_a.bin";
static const char* SLOT_B = "/relay_state_b.bin";

static bool readSlot(const char* path, PersistedRelayBlock& block, uint32_t& seq) {
    File f = LittleFS.open(path, "r");
    if (!f) return false;
    uint8_t buf[256];
    size_t n = f.read(buf, sizeof(buf));
    f.close();
    if (!decodeRelayState(buf, n, block, seq)) return false;
    return true;
}

RelayStateLoadResult loadRelayStateFromFS() {
    RelayStateLoadResult res{false, {}, 0};
    if (!ensureFS()) return res; // silent first boot
    PersistedRelayBlock aBlock{}, bBlock{};
    uint32_t aSeq=0,bSeq=0; bool aOk = readSlot(SLOT_A, aBlock, aSeq); bool bOk = readSlot(SLOT_B, bBlock, bSeq);
    if (!aOk && !bOk) return res;
    if (aOk && (!bOk || aSeq >= bSeq)) { res.success = true; res.block = aBlock; res.seq = aSeq; return res; }
    if (bOk) { res.success = true; res.block = bBlock; res.seq = bSeq; }
    return res;
}

bool saveRelayStateToFS(const PersistedRelayBlock& block, uint32_t previousSeq) {
    if (!ensureFS()) return false;
    uint8_t buf[256]; uint32_t crc=0; uint32_t seq = previousSeq + 1;
    size_t n = encodeRelayState(block, seq, buf, sizeof(buf), crc);
    if (n == 0) return false;
    // choose target slot (alternate)
    const char* target = (seq % 2 == 0) ? SLOT_A : SLOT_B;
    const char* temp = "/relay_state_tmp.bin";
    // Write temp
    File f = LittleFS.open(temp, "w"); if (!f) return false; size_t w = f.write(buf, n); f.close(); if (w != n) { LittleFS.remove(temp); return false; }
    // Atomic replace
    LittleFS.remove(target);
    if (!LittleFS.rename(temp, target)) { LittleFS.remove(temp); return false; }
    return true;
}
#endif
