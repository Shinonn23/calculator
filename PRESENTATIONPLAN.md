# Presentation Script — cmath-solver
# วีดีโอ: ไม่เกิน 8 นาที | ภาษา: ไทย
# ไม่ต้องแนะนำสมาชิก | ไม่ต้องแสดง Source Code | ไม่ต้องอธิบาย Background

---

## ส่วนที่ 1 — ภาพรวมโปรแกรม [~1:00 นาที]

**[เปิดหน้าจอ terminal, เรียกโปรแกรม `./cmath-solver`]**

> "cmath-solver คือโปรแกรม command-line ที่ทำหน้าที่เป็น interactive calculator
> พัฒนาด้วย C++ รองรับทั้ง REPL mode และ script mode
> มีความสามารถตั้งแต่คำนวณ expression ธรรมดา ไปจนถึงแก้สมการพหุนามหลายตัวแปร
> มาเริ่มดู Demo กันเลย"

---

## ส่วนที่ 2 — Expression และ Variables [~1:30 นาที]

**[พิมพ์ใน REPL]**

> "เริ่มต้นที่ฟีเจอร์พื้นฐาน — คำนวณ expression ได้ทันที"

```
2 + 3 * 4
(10 - 2) / 4
2 ^ 8
sin(pi/2)
cos(0)
tan(pi/4)
```

> "รองรับ operator ทั้งหมด รวมถึงฟังก์ชัน trigonometric และค่าคงที่อย่าง pi กับ e"

> "นอกจากคำนวณตรงๆ ยังตั้งค่าตัวแปรได้ด้วย `:set`"

```
:set x 5
:set y x * 2
y
```

> "x = 5, y = x*2 ดังนั้น y = 10
> แต่ที่พิเศษกว่านั้น ตัวแปรใน cmath-solver เป็น lazy evaluation
> ถ้าเปลี่ยน x ทีหลัง y จะอัปเดตตามอัตโนมัติ"

> "ใส่ expression ใน quote ได้ด้วย — มีประโยชน์เมื่อ expression ซับซ้อน"

```
:set f "x^2 + 3*x - 4"
f
```

```
:set x 10
y
```

> "y กลายเป็น 20 ทันที เพราะ y ผูกกับ expression `x * 2` ไม่ใช่ค่าตัวเลข"

> "ดูตัวแปรทั้งหมดใน context ด้วย `:ls` ลบตัวแปรด้วย `:unset`"

```
:ls
:unset y
```

---

## ส่วนที่ 3 — แก้สมการ [~2:00 นาที]

> "ฟีเจอร์หลักของโปรแกรมคือคำสั่ง `:solve` — แก้สมการอัตโนมัติ"

**Linear:**
```
:solve 2*x + 4 = 10
```
> "สมการเชิงเส้น — ได้ x = 3 และบันทึกลง context อัตโนมัติ"

**Quoted expression + post-expression flag (ใหม่):**
```
:solve "x^2 - 5*x + 6 = 0" --fraction
```
> "คำสั่ง solve รองรับ quoted expression ด้วย — ใส่ flag ไว้หลังสมการได้เลย
> ตัวเลขออกมาเป็น fraction แบบ exact"

**Quadratic (unquoted):**
```
:solve x^2 - 5*x + 6 = 0
```
> "แบบ unquoted ก็ยังใช้ได้เหมือนเดิม — ได้ x = [2, 3] ทั้งสองคำตอบ
> ถ้า x มีหลายค่า เราสามารถใช้ x ในสูตรต่อได้เลย"

```
x^2 + 1
```
> "ได้ผลลัพธ์เป็น [5, 10] — broadcast evaluation ทำงานกับทุกคำตอบพร้อมกัน"

**Cubic:**
```
:solve x^3 - 6*x^2 + 11*x - 6 = 0
```
> "สมการกำลังสาม — โปรแกรมใช้ numerical method (Durand-Kerner) ได้คำตอบ x = [1, 2, 3]"

**Error handling:**
```
:solve x^2 + 1 = 0
```
> "ถ้าไม่มีคำตอบจริง โปรแกรมแจ้ง error พร้อม message บอกเหตุผลชัดเจน"

---

## ส่วนที่ 4 — ระบบสมการหลายตัวแปร [~1:00 นาที]

> "นอกจากสมการเดี่ยว ยังแก้ระบบสมการ (systems of equations) ได้ด้วย
> คั่นแต่ละสมการด้วยเครื่องหมาย `;`"

```
:solve x + y = 5; x - y = 1
```
> "ได้ x = 3, y = 2 — บันทึกทั้งคู่ลง context"

```
:solve x + y + z = 6; 2*x - y + z = 3; x + 2*y - z = 2
```
> "ระบบ 3 ตัวแปร — โปรแกรมใช้ Gaussian Elimination หรือ LU decomposition
> มี option `--show-matrix` ให้ดู augmented matrix, `--exact` ให้ผลเป็น fraction"

```
:solve x + y + z = 6; 2*x - y + z = 3; x + 2*y - z = 2 --show-matrix
```

---

## ส่วนที่ 5 — Polynomial Operations [~0:45 นาที]

> "โปรแกรมทำงานกับพหุนามได้โดยตรง"

**Expand:**
```
:expand (x + 1)^3
:expand (x - 2)*(x + 2)
```
> "กระจายนิพจน์ให้อัตโนมัติ"

**Factor:**
```
:factor x^2 - 4
:factor x^2 - 4*x + 4
```
> "แยกตัวประกอบ — ตรวจจับ perfect square ด้วย"

**Simplify:**
```
:simplify 2*x + 3*x + 1 = 10
```
> "รวมเทอมเหมือนกัน จัดรูปสมการให้เรียบร้อย"

---

## ส่วนที่ 6 — Advanced Features [~1:00 นาที]

> "นอกจากคณิตศาสตร์ โปรแกรมมีระบบช่วยจัดการ workflow"

**Environments — บันทึกและโหลดชุดตัวแปร:**
```
:set a 10
:set b 20
:env save mywork
:env list
:env load mywork
```
> "เซฟ context ปัจจุบันไว้ใช้ทีหลัง — เหมาะสำหรับสลับระหว่างชุดปัญหาต่างกัน"

**Script Mode — รันไฟล์สคริปต์:**
```
:load setup.msl
```
> "เขียน command ไว้ใน .msl file แล้วรันทีเดียว รองรับ `--strict`, `--dry-run`, `--env`"

**History — ย้อนดูและรันซ้ำ:**
```
:history 10
:history search "solve"
:redo 3
```
> "ดู history ย้อนหลัง ค้นหา แล้วรันซ้ำ command เดิมได้เลย"

**Config:**
```
:config list
:config set output.decimals 3
```
> "ปรับ precision, fraction mode, theme และอื่นๆ — บันทึกถาวรลงไฟล์ config"

---

## ส่วนที่ 7 — สรุป [~0:15 นาที]

> "cmath-solver รองรับทั้ง expression evaluation, variable management แบบ lazy,
> การแก้สมการตั้งแต่ linear ถึง polynomial, ระบบสมการ NxN,
> polynomial expand/factor, scripting และ environment management
> command grammar รองรับทั้ง unquoted และ quoted expression — วาง flag ก่อนหรือหลัง expression ก็ได้
> ทั้งหมดนี้อยู่ใน CLI เดียว พร้อม error message ที่อ่านเข้าใจง่าย"

---

## หมายเหตุ Demo

- เปิด terminal ที่ project root
- รัน `./build/bin/cmath-solver` เพื่อเข้า REPL
- ลำดับคำสั่งตาม script ด้านบน
- เน้น feature ที่น่าสนใจ: lazy eval, broadcast, error handling
- Grammar ใหม่: `:solve "expr" [flags]` และ `:set x "expr"` — flags วางหลัง quoted expression ได้
