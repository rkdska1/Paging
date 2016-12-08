int foo(int val) {
	if (val <= 1) return 1;
	return (foo(val-1)*val);
}
