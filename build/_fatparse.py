import struct

# Parse the calculator's FAT volume raw to compare the on-disk shape of
# G2048.DAT (legacy 92-byte save, suspected pathological) vs MINESPB.DAT
# (1032-byte minesweeper save, known to read fine).
try:
    dev = '\\\\.\\E:'
    with open(dev, 'rb') as f:
        b = f.read(512)
    bps = struct.unpack_from('<H', b, 11)[0]
    spc = b[13]
    res = struct.unpack_from('<H', b, 14)[0]
    nf = b[16]
    rootent = struct.unpack_from('<H', b, 17)[0]
    fatsz = struct.unpack_from('<H', b, 22)[0]
    print('fs: bps=%d spc=%d reserved=%d fats=%d rootents=%d fatsize=%d'
          % (bps, spc, res, nf, rootent, fatsz))
    with open(dev, 'rb') as f:
        f.seek(res * bps)
        fat = f.read(fatsz * bps)
        f.seek((res + nf * fatsz) * bps)
        root = f.read(rootent * 32)

    def chain(s):
        out = []
        n = s
        while n and n < 0xFFF8 and len(out) < 40:
            out.append(n)
            n = struct.unpack_from('<H', fat, n * 2)[0]
        return out

    for i in range(rootent):
        e = root[i * 32:(i + 1) * 32]
        if e[0] in (0x00, 0xE5):
            continue
        nm = e[0:11].decode('ascii', 'replace')
        if nm.startswith('G2048') or nm.startswith('MINESPB'):
            cl = struct.unpack_from('<H', e, 26)[0]
            sz = struct.unpack_from('<I', e, 28)[0]
            ch = chain(cl)
            print('%-12s size=%-6d start_cluster=%-5d chain_len=%d chain=%s'
                  % (nm, sz, cl, len(ch), ch))
except Exception as ex:
    print('raw parse failed:', ex)
