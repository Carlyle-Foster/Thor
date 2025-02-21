union { int, f32, a.b, ^int, [^]T, []f32, []f32, [?]f32, x.y(10) }

/*
package ks

import "core:fmt"

Item :: struct {
	name:   string,
	weight: int,
	value:  int
}

T :: struct {
	x, y: int,
	z: f32
}

ks :: proc(items: []Item, w: int) -> []bool {
	n  := len(items);
	mm := make([]int, (n + 1) * (w + 1))
	m  := make([][]int, (n + 1))
	defer { delete(m); delete(mm) }
	m[0] = mm[:]
	for i in 1..=n {
		m[i] = mm[i * (w + 1):]
		for j in 0..=w {
			if items[i - 1].weight > j {
				m[i][j] = m[i - 1][j]
			} else {
				a := m[i - 1][j]
				b := m[i - 1][j - items[i - 1].weight] + items[i - 1].value
				m[i][j] = max(a, b)
			}
		}
	}
	s := make([]bool, n)
	for i, j := n, w; i > 0; i -= 1 {
		if m[i][j] > m[i - 1][j] {
			s[i - 1] = true
			j -= items[i - 1].weight
		}
	}
	return s
}

main :: proc() {
	items := []Item {
		{"map",                      9,   150},
		{"compass",                 13,    35},
		{"water",                  153,   200},
		{"sandwich",                50,   160},
		{"glucose",                 15,    60},
		{"tin",                     68,    45},
		{"banana",                  27,    60},
		{"apple",                   39,    40},
		{"cheese",                  23,    30},
		{"beer",                    52,    10},
		{"suntan cream",            11,    70},
		{"camera",                  32,    30},
		{"T-shirt",                 24,    15},
		{"trousers",                48,    10},
		{"umbrella",                73,    40},
		{"waterproof trousers",     42,    70},
		{"waterproof overclothes",  43,    75},
		{"note-case",               22,    80},
		{"sunglasses",               7,    20},
		{"towel",                   18,    12},
		{"socks",                    4,    50},
		{"book",                    30,    10}
	}
	s  := ks(items, 400)
	tw := 0
	tv := 0
	for item, i in items {
		s[i] or_continue
		fmt.printf("%-22s % 5d % 5d\n", item.name, item.weight, item.value)
		tw += item.weight
		tv += item.value
	}
	fmt.printf("%-22s % 5d % 5d\n", "totals:", tw, tv)
}
*/