BEGIN {
	ni = ARGV[1]
	t = ARGV[2]
	expected = ARGV[3]
	total = ARGV[4]
	n = ARGV[5]

	f = expected / total / (n - 1)
	if (expected == 0) {
		print 0
		exit
	}

	bisect = 0
	lb = 0
	ub = 1
	nhit = 0
	for (i = 0; i < ni; ++i) {
		cmd = t " -s " expected " -f " f
		cmd | getline
		cmd | getline
		close(cmd)

		thief_time = 0
		for (j = 2; j <= n; ++j)
			thief_time += $j
		thief_time /= n - 1

		err = thief_time / $1
		printf "%g [%g;%g] => %f / %f (%f)\n",
		    f, lb, ub, thief_time, $1, err | "cat >&2"
		diff = err < 1 ? 1 - err : err - 1

		nhit = diff < 0.05 ? nhit + 1 : 0
		if (nhit >= 2)
			break

		if (bisect) {
			if (err < 1)
				ub = f
			else
				lb = f
			f = (lb + ub) / 2
		} else {
			if (err < 1 && f < ub)
				ub = f
			if (err > 1 && f > lb)
				lb = f
			if (lb > 0 && ub < 1)
				bisect = 1
			f *= err
		}
	}

	print f
	exit
}
