#include <jni.h>

#include <cryptopp/aes.h>
#include <cryptopp/ccm.h>
#include <cryptopp/filters.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
constexpr std::size_t kCcmTagSize = 16;

std::uint32_t readLe32(const Bytes& data, std::size_t pos)
{
    if (pos + 4 > data.size()) {
        throw std::runtime_error("unexpected end of data");
    }
    return static_cast<std::uint32_t>(data[pos]) |
           (static_cast<std::uint32_t>(data[pos + 1]) << 8) |
           (static_cast<std::uint32_t>(data[pos + 2]) << 16) |
           (static_cast<std::uint32_t>(data[pos + 3]) << 24);
}

void appendLe32(Bytes& out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>(value & 0xff));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
}

std::uint32_t crc32(const Bytes& data)
{
    std::uint32_t crc = 0xffffffffu;
    for (std::uint8_t byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xedb88320u) : (crc >> 1);
        }
    }
    return crc ^ 0xffffffffu;
}

class Xorshift {
public:
    explicit Xorshift(std::uint32_t seed)
    {
        initialize(seed);
    }

    std::uint32_t next(std::uint32_t divisor = 0)
    {
        const std::uint32_t t = state_[0] ^ (state_[0] << 11);
        state_[0] = state_[1];
        state_[1] = state_[2];
        state_[2] = state_[3];
        state_[3] = (state_[3] ^ (state_[3] >> 19)) ^ (t ^ (t >> 8));
        return divisor > 0 ? state_[3] % divisor : state_[3];
    }

private:
    void initialize(std::uint32_t seed)
    {
        state_.fill(0);
        if (seed == 0) {
            return;
        }
        seed = (seed ^ (seed >> 30)) * (0x6C078966u - 1u) + 1u;
        state_[0] = seed;
        seed = (seed ^ (seed >> 30)) * (0x6C078966u - 1u) + 2u;
        state_[1] = seed;
        seed = (seed ^ (seed >> 30)) * (0x6C078966u - 1u) + 3u;
        state_[2] = seed;
        state_[3] = 0x03DF95B3u;
    }

    std::array<std::uint32_t, 4> state_{};
};

std::array<int, 256> oddPrimes()
{
    std::array<int, 256> result{};
    int count = 0;
    for (int candidate = 3; count < static_cast<int>(result.size()); candidate += 2) {
        bool prime = true;
        for (int divisor = 3; divisor * divisor <= candidate; divisor += 2) {
            if (candidate % divisor == 0) {
                prime = false;
                break;
            }
        }
        if (prime) {
            result[static_cast<std::size_t>(count++)] = candidate;
        }
    }
    return result;
}

class YwCipher {
public:
    explicit YwCipher(std::uint32_t seed, int count = 0x1000)
        : rng_(seed)
    {
        for (int i = 0; i < 256; ++i) {
            table_[static_cast<std::size_t>(i)] = i;
        }
        for (int i = 0; i < count; ++i) {
            const int r = static_cast<int>(rng_.next(0x10000));
            int r1 = r & 0xff;
            int r2 = (r >> 8) & 0xff;
            if (r1 != r2) {
                r1 = table_[static_cast<std::size_t>(r1)];
                r2 = table_[static_cast<std::size_t>(r2)];
                std::swap(table_[static_cast<std::size_t>(r1)], table_[static_cast<std::size_t>(r2)]);
            }
        }
    }

    Bytes transform(const Bytes& input) const
    {
        static const std::array<int, 256> primes = oddPrimes();
        Bytes output(input.size());
        int ka = 0;
        for (std::size_t index = 0; index < input.size(); ++index) {
            if ((index & 0xffu) == 0) {
                const std::size_t page = (index & 0xff00u) >> 8;
                ka = primes[static_cast<std::size_t>(table_[page])];
            }
            const int kb = table_[static_cast<std::size_t>((ka * static_cast<int>(index + 1)) & 0xff)];
            output[index] = static_cast<std::uint8_t>(input[index] ^ static_cast<std::uint8_t>(kb));
        }
        return output;
    }

private:
    Xorshift rng_;
    std::array<int, 256> table_{};
};

struct YwLayer {
    Bytes plain;
    std::array<std::uint8_t, 4> key{};
};

YwLayer decryptYwLayer(const Bytes& input)
{
    if (input.size() < 8) {
        throw std::runtime_error("YWCipher layer is too short");
    }
    const std::size_t bodySize = input.size() - 8;
    Bytes cipher(input.begin(), input.begin() + static_cast<std::ptrdiff_t>(bodySize));
    const std::uint32_t storedCrc = readLe32(input, bodySize);
    if (crc32(cipher) != storedCrc) {
        throw std::runtime_error("YWCipher CRC32 mismatch");
    }
    const std::uint32_t keyValue = readLe32(input, bodySize + 4);
    YwCipher cipherEngine(keyValue);
    YwLayer layer;
    layer.plain = cipherEngine.transform(cipher);
    std::copy_n(input.begin() + static_cast<std::ptrdiff_t>(bodySize + 4), 4, layer.key.begin());
    return layer;
}

Bytes encryptYwLayer(const Bytes& plain, const std::array<std::uint8_t, 4>& key)
{
    const std::uint32_t keyValue = static_cast<std::uint32_t>(key[0]) |
                                   (static_cast<std::uint32_t>(key[1]) << 8) |
                                   (static_cast<std::uint32_t>(key[2]) << 16) |
                                   (static_cast<std::uint32_t>(key[3]) << 24);
    YwCipher cipherEngine(keyValue);
    Bytes encrypted = cipherEngine.transform(plain);
    const std::uint32_t checksum = crc32(encrypted);
    appendLe32(encrypted, checksum);
    encrypted.insert(encrypted.end(), key.begin(), key.end());
    return encrypted;
}

struct SectionInfo {
    int count = 0;
    int maxDepth = 0;
    int rootId = -1;
};

SectionInfo parseSections(const Bytes& data)
{
    if (data.size() < 8) {
        throw std::runtime_error("section data is too short");
    }

    std::size_t pos = 0;
    std::uint32_t h1 = readLe32(data, pos);
    pos += 4;
    if ((h1 & 0xffffu) != 0xfffeu) {
        throw std::runtime_error("section magic 0xFFFE not found");
    }
    const std::uint32_t rootHeader = readLe32(data, pos);
    pos += 4;
    const std::uint32_t rootSize = rootHeader >> 8;
    if (rootSize < 4 || pos + rootSize > data.size()) {
        throw std::runtime_error("invalid root section size");
    }

    SectionInfo info;
    info.count = 1;
    info.maxDepth = 1;
    info.rootId = static_cast<int>(rootHeader & 0xffu);
    int depth = 1;

    while (pos < data.size()) {
        if (pos + 4 > data.size()) {
            throw std::runtime_error("truncated section footer");
        }
        h1 = readLe32(data, pos);
        pos += 4;
        std::uint32_t size = 4;

        while ((h1 & 0xffffu) == 0xfffeu) {
            if (pos + 4 > data.size()) {
                throw std::runtime_error("truncated section header");
            }
            const std::uint32_t h2 = readLe32(data, pos);
            pos += 4;
            size = h2 >> 8;
            if (depth <= 0) {
                throw std::runtime_error("section parent stack underflow");
            }
            if (size < 4 || pos + size > data.size()) {
                throw std::runtime_error("invalid child section size");
            }
            ++depth;
            ++info.count;
            info.maxDepth = std::max(info.maxDepth, depth);
            if (pos + 4 > data.size()) {
                throw std::runtime_error("truncated nested section");
            }
            h1 = readLe32(data, pos);
            pos += 4;
        }

        if ((h1 & 0xffffu) == 0xfeffu) {
            --depth;
            if (depth < 0) {
                throw std::runtime_error("section footer stack underflow");
            }
        }

        const std::size_t skip = static_cast<std::size_t>(size - 4);
        if (pos + skip > data.size()) {
            throw std::runtime_error("section body exceeds file size");
        }
        pos += skip;
    }

    if (depth != 0) {
        throw std::runtime_error("unclosed section tree");
    }
    return info;
}

bool hasSectionMagic(const Bytes& data, std::size_t offset)
{
    if (offset + 4 > data.size()) {
        return false;
    }
    return (readLe32(data, offset) & 0xffffu) == 0xfffeu;
}

Bytes ccmDecrypt(const Bytes& input, const Bytes& key, const Bytes& nonce)
{
    if (input.size() < kCcmTagSize || key.size() != 16 || nonce.empty()) {
        throw std::runtime_error("invalid AES-CCM input");
    }

    std::string combined(
        reinterpret_cast<const char*>(input.data() + kCcmTagSize),
        input.size() - kCcmTagSize);
    combined.append(reinterpret_cast<const char*>(input.data()), kCcmTagSize);

    std::string output;
    try {
        CryptoPP::CCM<CryptoPP::AES, kCcmTagSize>::Decryption decryptor;
        decryptor.SetKeyWithIV(key.data(), key.size(), nonce.data(), nonce.size());
        decryptor.SpecifyDataLengths(0, combined.size() - kCcmTagSize, 0);
        CryptoPP::AuthenticatedDecryptionFilter filter(
            decryptor,
            new CryptoPP::StringSink(output),
            CryptoPP::AuthenticatedDecryptionFilter::THROW_EXCEPTION);
        CryptoPP::StringSource source(combined, true, new CryptoPP::Redirector(filter));
    } catch (const CryptoPP::Exception&) {
        throw std::runtime_error("AES-CCM authentication failed");
    }
    return Bytes(output.begin(), output.end());
}

Bytes ccmEncrypt(const Bytes& input, const Bytes& key, const Bytes& nonce)
{
    if (key.size() != 16 || nonce.empty()) {
        throw std::runtime_error("invalid AES-CCM key or nonce");
    }

    const std::string plain(reinterpret_cast<const char*>(input.data()), input.size());
    std::string output;
    try {
        CryptoPP::CCM<CryptoPP::AES, kCcmTagSize>::Encryption encryptor;
        encryptor.SetKeyWithIV(key.data(), key.size(), nonce.data(), nonce.size());
        encryptor.SpecifyDataLengths(0, plain.size(), 0);
        CryptoPP::StringSource source(
            plain,
            true,
            new CryptoPP::AuthenticatedEncryptionFilter(
                encryptor,
                new CryptoPP::StringSink(output)));
    } catch (const CryptoPP::Exception&) {
        throw std::runtime_error("AES-CCM encryption failed");
    }

    if (output.size() < kCcmTagSize) {
        throw std::runtime_error("AES-CCM output is too short");
    }
    const std::size_t cipherSize = output.size() - kCcmTagSize;
    Bytes result;
    result.reserve(output.size());
    result.insert(
        result.end(),
        reinterpret_cast<const std::uint8_t*>(output.data() + static_cast<std::ptrdiff_t>(cipherSize)),
        reinterpret_cast<const std::uint8_t*>(output.data() + output.size()));
    result.insert(
        result.end(),
        reinterpret_cast<const std::uint8_t*>(output.data()),
        reinterpret_cast<const std::uint8_t*>(output.data() + static_cast<std::ptrdiff_t>(cipherSize)));
    return result;
}

Bytes asciiBytes(const char* text)
{
    const std::string value(text);
    return Bytes(value.begin(), value.end());
}

Bytes generateYw2HeadKey(const Bytes& rawHead)
{
    const YwLayer head = decryptYwLayer(rawHead);
    const std::uint32_t seed = readLe32(head.plain, 0x0c);
    Xorshift rng(seed);
    Bytes key;
    key.reserve(16);
    for (int i = 0; i < 16; ++i) {
        key.push_back(static_cast<std::uint8_t>(rng.next(0x100)));
    }
    return key;
}

std::uint32_t bustersSub(const Bytes& head, int i, int index, bool nonJp)
{
    if (index > 0) {
        --index;
    }
    const int userLength = nonJp ? 0x80 : 0x78;
    const int ignLength = nonJp ? 0x1c : 0x18;
    const std::size_t pos = static_cast<std::size_t>(index * userLength + 0x39c8 + ignLength + i * 4);
    return readLe32(head, pos);
}

Bytes generateBustersKey(const Bytes& decryptedHead, int index, bool nonJp)
{
    std::uint32_t seed = readLe32(decryptedHead, 0x0c);
    seed ^= bustersSub(decryptedHead, 0x0c, index, nonJp);
    if ((bustersSub(decryptedHead, 0, index, nonJp) & 0x4000u) != 0) {
        seed = ~seed;
    }

    Xorshift rng(seed);
    const int warmups = static_cast<int>(bustersSub(decryptedHead, 0x0a, index, nonJp) & 0xffu);
    for (int i = 0; i < warmups; ++i) {
        rng.next();
    }

    Bytes key;
    key.reserve(16);
    for (int i = 0; i < 16; ++i) {
        key.push_back(static_cast<std::uint8_t>(rng.next(0x100)));
    }
    return key;
}

struct DecodedSave {
    Bytes original;
    Bytes body;
    std::array<std::uint8_t, 4> ywKey{};
    Bytes aesKey;
    Bytes nonce;
    Bytes prefix16;
    SectionInfo sections;
    std::string format;
    bool encrypted = false;
};

DecodedSave decodedDirectBody(const Bytes& input, const std::string& format)
{
    DecodedSave decoded;
    decoded.original = input;
    decoded.format = format;

    std::vector<Bytes> candidates;
    candidates.push_back(input);
    if (input.size() > 4) {
        candidates.emplace_back(input.begin(), input.end() - 4);
    }

    std::string lastError;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        try {
            const SectionInfo info = parseSections(candidates[i]);
            decoded.body = std::move(candidates[i]);
            decoded.sections = info;
            if (i == 1) {
                std::copy_n(input.end() - 4, 4, decoded.ywKey.begin());
            }
            return decoded;
        } catch (const std::exception& e) {
            lastError = e.what();
        }
    }
    throw std::runtime_error("decrypted section parse failed: " + lastError);
}

DecodedSave decodeYw1(const Bytes& input)
{
    if (hasSectionMagic(input, 0)) {
        return decodedDirectBody(input, "decrypted-yw1");
    }

    const YwLayer layer = decryptYwLayer(input);
    DecodedSave decoded;
    decoded.original = input;
    decoded.body = layer.plain;
    decoded.ywKey = layer.key;
    decoded.sections = parseSections(decoded.body);
    decoded.format = "encrypted-yw1";
    decoded.encrypted = true;
    return decoded;
}

DecodedSave decodeDecryptedAesContainer(const Bytes& input, const std::string& format)
{
    if (input.size() < 36 || !hasSectionMagic(input, 32)) {
        throw std::runtime_error("not a decrypted AES container");
    }
    DecodedSave decoded;
    decoded.original = input;
    decoded.format = format;
    decoded.nonce.assign(input.begin(), input.begin() + 12);
    decoded.prefix16.assign(input.begin(), input.begin() + 16);
    decoded.aesKey.assign(input.begin() + 16, input.begin() + 32);
    decoded.body.assign(input.begin() + 32, input.end() - 4);
    std::copy_n(input.end() - 4, 4, decoded.ywKey.begin());
    decoded.sections = parseSections(decoded.body);
    return decoded;
}

DecodedSave decodeAesEncrypted(
    const Bytes& input,
    const std::vector<Bytes>& candidateKeys,
    const std::string& format)
{
    if (input.size() <= 32) {
        throw std::runtime_error("encrypted save is too short");
    }
    const Bytes nonce(input.begin(), input.begin() + 12);
    const Bytes prefix16(input.begin(), input.begin() + 16);
    const Bytes encryptedFirst(input.begin() + 16, input.end());

    std::string lastError = "no key candidates";
    for (const Bytes& key : candidateKeys) {
        try {
            const Bytes secondLayer = ccmDecrypt(encryptedFirst, key, nonce);
            const YwLayer layer = decryptYwLayer(secondLayer);
            const SectionInfo info = parseSections(layer.plain);

            DecodedSave decoded;
            decoded.original = input;
            decoded.body = layer.plain;
            decoded.ywKey = layer.key;
            decoded.aesKey = key;
            decoded.nonce = nonce;
            decoded.prefix16 = prefix16;
            decoded.sections = info;
            decoded.format = format;
            decoded.encrypted = true;
            return decoded;
        } catch (const std::exception& e) {
            lastError = e.what();
        }
    }
    throw std::runtime_error("AES/YWCipher decode failed: " + lastError);
}

DecodedSave decodeYw2(const Bytes& input, const std::optional<Bytes>& head)
{
    if (input.size() >= 36 && hasSectionMagic(input, 32)) {
        return decodeDecryptedAesContainer(input, "decrypted-yw2");
    }

    std::vector<Bytes> keys;
    keys.push_back(asciiBytes("5+NI8WVq09V7LI5w"));
    if (head.has_value()) {
        keys.push_back(generateYw2HeadKey(*head));
    }
    return decodeAesEncrypted(input, keys, "encrypted-yw2");
}

DecodedSave decodeBusters(const Bytes& input, const std::optional<Bytes>& head)
{
    if (input.size() >= 36 && hasSectionMagic(input, 32)) {
        return decodeDecryptedAesContainer(input, "decrypted-busters");
    }
    if (!head.has_value()) {
        throw std::runtime_error("Busters encrypted save requires head.yw or head.yw_g");
    }

    const YwLayer decryptedHead = decryptYwLayer(*head);
    const bool nonJp = head->size() >= 15180;
    std::vector<Bytes> keys;
    keys.reserve(3);
    for (int index = 1; index <= 3; ++index) {
        keys.push_back(generateBustersKey(decryptedHead.plain, index, nonJp));
    }
    return decodeAesEncrypted(input, keys, "encrypted-busters");
}

DecodedSave decodeSave(int gameId, const Bytes& input, const std::optional<Bytes>& head)
{
    switch (gameId) {
    case 1:
        return decodeYw1(input);
    case 2:
        return decodeYw2(input, head);
    case 3:
        return decodeBusters(input, head);
    default:
        throw std::runtime_error("unknown game id");
    }
}

Bytes roundTrip(const DecodedSave& decoded)
{
    if (!decoded.encrypted) {
        return decoded.original;
    }

    const Bytes secondLayer = encryptYwLayer(decoded.body, decoded.ywKey);
    if (decoded.aesKey.empty()) {
        return secondLayer;
    }

    Bytes result = decoded.prefix16;
    const Bytes encryptedFirst = ccmEncrypt(secondLayer, decoded.aesKey, decoded.nonce);
    result.insert(result.end(), encryptedFirst.begin(), encryptedFirst.end());
    return result;
}

std::string gameName(int gameId)
{
    switch (gameId) {
    case 1: return "Yo-kai Watch 1";
    case 2: return "Yo-kai Watch 2";
    case 3: return "Yo-kai Watch Busters";
    default: return "Unknown";
    }
}

std::string jsonEscape(const std::string& input)
{
    std::string out;
    out.reserve(input.size() + 8);
    for (char ch : input) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += ch; break;
        }
    }
    return out;
}

std::string inspectJson(int gameId, const Bytes& input, const std::optional<Bytes>& head)
{
    try {
        const DecodedSave decoded = decodeSave(gameId, input, head);
        std::ostringstream out;
        out << "{\"ok\":true"
            << ",\"game\":\"" << jsonEscape(gameName(gameId)) << "\""
            << ",\"format\":\"" << jsonEscape(decoded.format) << "\""
            << ",\"size\":" << input.size()
            << ",\"sections\":" << decoded.sections.count
            << ",\"maxDepth\":" << decoded.sections.maxDepth
            << ",\"rootId\":" << decoded.sections.rootId
            << ",\"roundTrip\":\"supported\"}";
        return out.str();
    } catch (const std::exception& e) {
        std::ostringstream out;
        out << "{\"ok\":false"
            << ",\"game\":\"" << jsonEscape(gameName(gameId)) << "\""
            << ",\"size\":" << input.size()
            << ",\"error\":\"" << jsonEscape(e.what()) << "\"}";
        return out.str();
    }
}

Bytes fromJByteArray(JNIEnv* env, jbyteArray array)
{
    if (array == nullptr) {
        return {};
    }
    const jsize length = env->GetArrayLength(array);
    Bytes result(static_cast<std::size_t>(length));
    if (length > 0) {
        env->GetByteArrayRegion(array, 0, length, reinterpret_cast<jbyte*>(result.data()));
    }
    return result;
}

jbyteArray toJByteArray(JNIEnv* env, const Bytes& data)
{
    jbyteArray result = env->NewByteArray(static_cast<jsize>(data.size()));
    if (result != nullptr && !data.empty()) {
        env->SetByteArrayRegion(
            result,
            0,
            static_cast<jsize>(data.size()),
            reinterpret_cast<const jbyte*>(data.data()));
    }
    return result;
}

std::optional<Bytes> optionalHead(JNIEnv* env, jbyteArray head)
{
    if (head == nullptr) {
        return std::nullopt;
    }
    return fromJByteArray(env, head);
}

void throwIllegalArgument(JNIEnv* env, const std::string& message)
{
    jclass cls = env->FindClass("java/lang/IllegalArgumentException");
    if (cls != nullptr) {
        env->ThrowNew(cls, message.c_str());
    }
}
} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_atuy_ykweditors_NativeSaveBridge_nativeInspect(
    JNIEnv* env,
    jobject,
    jint gameId,
    jbyteArray data,
    jbyteArray headData)
{
    const Bytes input = fromJByteArray(env, data);
    const std::string result = inspectJson(static_cast<int>(gameId), input, optionalHead(env, headData));
    return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_atuy_ykweditors_NativeSaveBridge_nativeRoundTrip(
    JNIEnv* env,
    jobject,
    jint gameId,
    jbyteArray data,
    jbyteArray headData)
{
    try {
        const Bytes input = fromJByteArray(env, data);
        const DecodedSave decoded = decodeSave(static_cast<int>(gameId), input, optionalHead(env, headData));
        return toJByteArray(env, roundTrip(decoded));
    } catch (const std::exception& e) {
        throwIllegalArgument(env, e.what());
        return nullptr;
    }
}
