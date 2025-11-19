import decimal

decimal.getcontext().prec = 100010

def compute_pi():
	one = decimal.Decimal(1)
	two = decimal.Decimal(2)
	four = decimal.Decimal(4)
	a = one
	b = one / decimal.Decimal(2).sqrt()
	t = decimal.Decimal(0.25)
	p = one
	for _ in range(20):
		a_next = (a + b) / two
		b = (a * b).sqrt()
		t -= p * (a - a_next) ** 2
		a = a_next
		p *= two
	pi = (a + b) ** 2 / (four * t)
	return pi

pi_val = compute_pi()
with open("pi.txt", "w") as f:
	f.write(str(pi_val))
