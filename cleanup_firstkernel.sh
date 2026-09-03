#!/usr/bin/env bash
# cleanup_firstkernel.sh
# รันจาก root ของโปรเจกต์ Firstkernel-main (โฟลเดอร์ที่มี Makefile)
set -e

if [ ! -f "Makefile" ] || [ ! -d "kernel" ]; then
    echo "[!] ต้องรันสคริปต์นี้จาก root ของโปรเจกต์ (โฟลเดอร์ที่มี Makefile กับ kernel/)"
    exit 1
fi

echo "[1/4] ลบไฟล์ .bak.vXXX ที่หลุดมาเพราะ .gitignore พัง..."
find . -type f -name "*.bak.v*" -print -delete

echo "[2/4] ลบ all_diffs.txt (dev log dump ที่ไม่ควรอยู่ใน repo)..."
[ -f "all_diffs.txt" ] && rm -v all_diffs.txt

echo "[3/4] แก้ .gitignore ให้ ignore ถูกต้อง..."
if [ -f ".gitignore" ]; then
    # ลบบรรทัดที่พังทิ้ง แล้วเติมบรรทัดที่ถูกต้อง (กันซ้ำด้วย grep -q)
    sed -i '/^all_diffs\.txt\*\.bak$/d' .gitignore
    grep -qxF 'all_diffs.txt' .gitignore || echo 'all_diffs.txt' >> .gitignore
    grep -qxF '*.bak' .gitignore       || echo '*.bak' >> .gitignore
    grep -qxF '*.bak.*' .gitignore     || echo '*.bak.*' >> .gitignore
    echo "    -> .gitignore แก้แล้ว"
else
    echo "    -> ไม่พบ .gitignore ข้ามขั้นตอนนี้"
fi

echo "[4/4] แก้ cast ซ้ำใน kernel/syscall.c..."
if [ -f "kernel/syscall.c" ]; then
    sed -i \
        's/(const void \*)(uintptr_t)(const void \*)(uintptr_t)user_buffer/(const void *)(uintptr_t)user_buffer/' \
        kernel/syscall.c
    echo "    -> kernel/syscall.c แก้แล้ว"
fi

echo ""
echo "เสร็จแล้วครับ เก็บกวาดครบ 4 อย่าง:"
echo "  - ลบไฟล์ .bak.vXXX ทั้งหมด"
echo "  - ลบ all_diffs.txt"
echo "  - แก้ .gitignore ให้ ignore ถูกจริง"
echo "  - แก้ cast ซ้ำใน syscall.c"
echo ""
echo "แนะนำ: รัน 'make clean && make' อีกครั้งเพื่อยืนยันว่ายัง build ผ่านปกติ"
