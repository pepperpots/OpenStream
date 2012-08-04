BEGIN {
	ni = ARGV[1]
	t = ARGV[2]
	expected = ARGV[3]
	total = ARGV[4]
	n = ARGV[5]
	safe_adj = 0.9

	f = expected / total / (n - 1)
	if (expected == 0) {
		print 0
		exit
	}
	f = f / safe_adj

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

		adj_worker_time = safe_adj * $1
		err = thief_time / adj_worker_time
		printf "%g [%g;%g] => %f / (%g * %f) = %f\n",
		    f, lb, ub, thief_time, safe_adj, $1, err | "cat >&2"
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
			if (lb >= ub)
				break
			else if (lb > 0 && ub < 1 || i >= ni / 2)
				bisect = 1
			f *= err
			if (f >= 1)
				f = 1
		}
	}

	print f
	exit
}
