// Host-side check using the real BearSSL X.509 engine (no Arduino or Wi-Fi).
#include <bearssl.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

using Bytes = std::vector<unsigned char>;

static Bytes read_file(const char *path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        std::exit(2);
    return Bytes(std::istreambuf_iterator<char>(file), {});
}

static void append_dn(void *ctx, const void *data, size_t size)
{
    auto &dn = *static_cast<Bytes *>(ctx);
    const auto *bytes = static_cast<const unsigned char *>(data);
    dn.insert(dn.end(), bytes, bytes + size);
}

struct Anchor
{
    Bytes dn;
    br_x509_decoder_context decoder;
    br_x509_trust_anchor ta = {};

    explicit Anchor(const char *path)
    {
        const Bytes der = read_file(path);
        br_x509_decoder_init(&decoder, append_dn, &dn);
        br_x509_decoder_push(&decoder, der.data(), der.size());
        const br_x509_pkey *key = br_x509_decoder_get_pkey(&decoder);
        if (!key || br_x509_decoder_last_error(&decoder))
            std::exit(2);
        ta.dn.data = dn.data();
        ta.dn.len = dn.size();
        ta.pkey = *key; // key buffers live in decoder, kept until validation ends
        ta.flags = br_x509_decoder_isCA(&decoder) ? BR_X509_TA_CA : 0;
    }
};

int main(int argc, char **argv)
{
    // unix_time hostname anchor_count anchor0.der [...] cert0.der [...]
    if (argc < 6)
        return 2;
    const int count = std::atoi(argv[3]);
    if (count < 1 || count > argc - 5)
        return 2;
    std::vector<std::unique_ptr<Anchor>> anchors;
    std::vector<br_x509_trust_anchor> tas;
    for (int i = 4; i < 4 + count; i++)
    {
        anchors.emplace_back(new Anchor(argv[i]));
        tas.push_back(anchors.back()->ta);
    }
    // The private cloud leaf must not accidentally become a certificate issuer.
    if (tas[0].flags != 0)
        return 2;
    for (int i = 1; i < count; i++)
        if (tas[i].flags != BR_X509_TA_CA)
            return 2;

    br_x509_minimal_context ctx;
    br_x509_minimal_init_full(&ctx, tas.data(), tas.size());
    const auto now = std::strtoull(argv[1], nullptr, 10);
    br_x509_minimal_set_time(&ctx, now / 86400 + 719528, now % 86400);
    ctx.vtable->start_chain(&ctx.vtable, argv[2]);
    for (int i = 4 + count; i < argc; i++)
    {
        const Bytes der = read_file(argv[i]);
        ctx.vtable->start_cert(&ctx.vtable, der.size());
        ctx.vtable->append(&ctx.vtable, der.data(), der.size());
        ctx.vtable->end_cert(&ctx.vtable);
    }
    const unsigned result = ctx.vtable->end_chain(&ctx.vtable);
    std::printf("%u\n", result);
    return 0;
}
