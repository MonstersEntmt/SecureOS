# Kernel Entry

## Architecture Pre Init

---
### x86-64 (AMD64)
- Setup GDT in format
```
    00: Null descriptor
    08: Code descriptor ring 0
    10: Data descriptor ring 0
    18: Code descriptor ring 3
    20: Data descriptor ring 3
```

- Setup IDT in format
```
    00 (#DE): Interrupt gate selector 08 ring 0
    06 (#UD): Interrupt gate selector 08 ring 0
    08 (#DF): Trap gate selector 08 ring 0
    0D (#GP): Trap gate selector 08 ring 0
    0E (#PF): Trap gate selector 08 ring 0
```

- Load GDT (cs 08, ds 10), LDT 00 and IDT

- Detect and enable Features
```
    1. Force enable NXE bit in EFER
    2. Force enable SSE and detect SSE extensions
    3. Detect AVX extensions
```
---

## Init

- Select and init PMM
- Select and init Kernel VMM
    - Kernel VMM shall have all physical addresses mapped 1:1.
- Reclaim PMM pages

## Architecture Post Init

---
### x86-64 (AMD64)
---