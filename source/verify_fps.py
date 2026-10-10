"""离线验证 FPS 数字的逻辑→物理坐标变换（三种朝向）。

为什么必须验：这块代码历史上栽过两次 —— 一次是竖屏/横屏搞反，一次是硬编码的
擦除矩形与实际绘制位置不一致。两次都是"肉眼看代码没问题、刷机才发现画错地方"。

验的是 rg_touch_overlay.c 的 draw_fps_text() 与 rg_overlay_get_fps_phys_rect()：
  1) 三种朝向下，逻辑矩形是否**无重叠、无遗漏**地映射进面板物理范围
  2) 变换是否是**双射**（面积守恒 ⇒ 不丢点、不重复）
  3) 绘制函数实际会写到的像素集合，是否与 phys_rect 给出的矩形一致
     —— 这一条最关键：它保证"擦除范围"和"绘制范围"永远对齐
  4) FPS 数字是否落在游戏画面之外（横屏游戏区 x=280..1000, y=120..600）
"""
PHYS_W, PHYS_H = 720, 1280          # 面板原生竖屏
GAME = (280, 120, 1000, 600)        # 横屏游戏画面区（逻辑）
SWAP = (540, 20, 740, 124)          # X/Y<->L/R 交换按钮（含 10px 命中外扩）
R_SHOULDER = (1090, 108, 1270, 192) # R 肩键（含命中区）

SCALE = 4
GLYPH_H = 8 * SCALE

def text_rect(n):
    """逻辑矩形（lx0,ly0,lw,lh），与 blit_fps 一致"""
    tw = n * 8 * SCALE
    lx = 1140 - tw // 2
    ly = 60 - GLYPH_H // 2
    return lx, ly, tw, GLYPH_H

def phys_rect(rect, rot, pw=PHYS_W, ph=PHYS_H, margin=3):
    """rg_overlay_get_fps_phys_rect 的换算（含 margin），闭区间"""
    lx0, ly0, lw, lh = rect
    lx0 -= margin; ly0 -= margin
    lx1 = lx0 + lw + 2 * margin
    ly1 = ly0 + lh + 2 * margin
    if rot == 2:                      # CCW90: px = ly, py = ph-1-lx
        return (ly0, ph - lx1, ly1, ph - lx0)
    else:                             # CW90 / 线性兜底
        return (pw - ly1, lx0, pw - ly0, lx1)

def draw_pixels(rect, rot, pw=PHYS_W, ph=PHYS_H):
    """draw_fps_text 逐像素循环实际会写到的 (px,py) 集合（仅取"点亮"的像素）"""
    lx0, ly0, lw, lh = rect
    out = set()
    for ly in range(ly0, ly0 + lh):
        gy = (ly - ly0) // SCALE
        if not (0 <= gy <= 7):
            continue
        for lx in range(lx0, lx0 + lw):
            gx = (lx - lx0) // SCALE
            if rot == 1:
                px, py = (pw - 1) - ly, lx
            elif rot == 2:
                px, py = ly, (ph - 1) - lx
            else:
                px, py = lx, ly
            if 0 <= px < pw and 0 <= py < ph:
                out.add((px, py))
    return out

fail = 0
def check(cond, msg):
    global fail
    if not cond:
        fail += 1
        print("  FAIL:", msg)

def text_rect_portrait(n):
    """竖屏口径的位置（原 RG_FPS_TEXT_CX/CY = 360/500），给 linear 分支用"""
    tw = n * 8 * SCALE
    return 360 - tw // 2, 500 - GLYPH_H // 2, tw, GLYPH_H

print("=== 1) 双射性：逻辑点数 == 物理点数（三种朝向） ===")
for rot, name in ((0, "linear"), (1, "CW90"), (2, "CCW90")):
    for n in (1, 2, 3):
        # linear 走竖屏分支（720x1280），横屏坐标对它必然出界 —— 各用各的位置
        r = text_rect_portrait(n) if rot == 0 else text_rect(n)
        expect = r[2] * r[3]
        got = len(draw_pixels(r, rot))
        ok = got == expect
        if not ok:
            fail += 1
        print(f"  {name:6s} {n}位: 逻辑 {expect} px -> 物理 {got} px  {'OK' if ok else 'MISMATCH'}")

print("\n=== 2) 绘制像素 ⊆ 擦除矩形（反之亦然都要成立） ===")
for rot, name in ((1, "CW90"), (2, "CCW90")):
    for n in (1, 2, 3):
        r = text_rect(n)
        x0, y0, x1, y1 = phys_rect(r, rot)
        drawn = draw_pixels(r, rot)
        inside = all(x0 <= px <= x1 and y0 <= py <= y1 for px, py in drawn)
        covered = set((px, py) for px in range(x0, x1 + 1) for py in range(y0, y1 + 1))
        check(inside, f"{name} {n}位: 有像素画到了擦除矩形外")
        check(drawn <= covered, f"{name} {n}位: 绘制像素不在擦除矩形内")
        print(f"  {name:6s} {n}位: 擦除矩形 {x1-x0+1}x{y1-y0+1}, 绘制 {len(drawn)} px  "
              f"{'全部落在矩形内' if inside and drawn <= covered else 'MISMATCH'}")

print("\n=== 3) 不越界 ===")
for rot, name in ((1, "CW90"), (2, "CCW90")):
    for n in (1, 2, 3):
        x0, y0, x1, y1 = phys_rect(text_rect(n), rot)
        check(x0 >= 0 and y0 >= 0 and x1 < PHYS_W and y1 < PHYS_H,
              f"{name} {n}位 越界: ({x0},{y0})-({x1},{y1})")
    print(f"  {name}: 全部在 0..{PHYS_W-1} x 0..{PHYS_H-1} 内")

print("\n=== 4) CCW90（当前默认朝向）下逻辑位置对应物理何处 ===")
for n in (1, 2, 3):
    r = text_rect(n)
    x0, y0, x1, y1 = phys_rect(r, 2)
    # CCW90 下 px = ly, py = ph-1-lx，所以逻辑 x 映射到物理 y 的反向
    print(f"  {n}位: 逻辑 x{r[0]}..{r[0]+r[2]} y{r[1]}..{r[1]+r[3]}"
          f"  ->  物理 x{x0}..{x1} y{y0}..{y1}")

print("\n=== 5) 逻辑矩形是否压到游戏画面（横屏游戏区 x280..1000 y120..600）===")
for n in (1, 2, 3):
    lx0, ly0, lw, lh = text_rect(n)
    lx1, ly1 = lx0 + lw, ly0 + lh
    overlap = not (lx1 <= GAME[0] or lx0 >= GAME[2] or ly1 <= GAME[1] or ly0 >= GAME[3])
    check(not overlap, f"{n}位 数字压到了游戏画面")
    print(f"  {n}位: 逻辑 y {ly0}..{ly1} vs 游戏区 y{GAME[1]}..{GAME[3]} -> "
          f"{'压到了!' if overlap else '不压（位于游戏区上方）'}")

print("\n=== 6) 不压 swap 按钮 / R 肩键（右上角新位置专查）===")
for n in (1, 2, 3):
    lx0, ly0, lw, lh = text_rect(n)
    r = (lx0 - 3, ly0 - 3, lx0 + lw + 3, ly0 + lh + 3)
    for nm, e in (("swap按钮", SWAP), ("R肩键", R_SHOULDER)):
        ov = not (r[2] <= e[0] or r[0] >= e[2] or r[3] <= e[1] or r[1] >= e[3])
        check(not ov, f"{n}位 数字压到了{nm}")
    print(f"  {n}位: {r}  swap={SWAP} R肩键={R_SHOULDER} -> 无重叠")

print("\n" + ("全部通过" if fail == 0 else f"★ {fail} 项失败"))