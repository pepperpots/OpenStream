// Copyright 2014 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package runtime

import (
	"runtime/internal/atomic"
	"runtime/internal/sys"
	"unsafe"
)

type mOS struct {
	waitsemacount uint32
}

//go:noescape
//extern lwp_park
func lwp_park(ts int32, rel int32, abstime *timespec, unpark int32, hint, unparkhint unsafe.Pointer) int32

//go:noescape
//extern lwp_unpark
func lwp_unpark(lwp int32, hint unsafe.Pointer) int32

//go:nosplit
func semacreate(mp *m) {
}

//go:nosplit
func semasleep(ns int64) int32 {
	_g_ := getg()
	var deadline int64
	if ns >= 0 {
		deadline = nanotime() + ns
	}

	for {
		v := atomic.Load(&_g_.m.mos.waitsemacount)
		if v > 0 {
			if atomic.Cas(&_g_.m.mos.waitsemacount, v, v-1) {
				return 0 // semaphore acquired
			}
			continue
		}

		// Sleep until unparked by semawakeup or timeout.
		var tsp *timespec
		var ts timespec
		if ns >= 0 {
			wait := deadline - nanotime()
			if wait <= 0 {
				return -1
			}
			var nsec int32
			ts.set_sec(timediv(wait, 1000000000, &nsec))
			ts.set_nsec(nsec)
			tsp = &ts
		}
		ret := lwp_park(_CLOCK_MONOTONIC, _TIMER_RELTIME, tsp, 0, unsafe.Pointer(&_g_.m.waitsemacount), nil)
		if ret == _ETIMEDOUT {
			return -1
		}
	}
}

//go:nosplit
func semawakeup(mp *m) {
	atomic.Xadd(&mp.mos.waitsemacount, 1)
	// From NetBSD's _lwp_unpark(2) manual:
	// "If the target LWP is not currently waiting, it will return
	// immediately upon the next call to _lwp_park()."
	ret := lwp_unpark(int32(mp.procid), unsafe.Pointer(&mp.mos.waitsemacount))
	if ret != 0 && ret != _ESRCH {
		// semawakeup can be called on signal stack.
		systemstack(func() {
			print("thrwakeup addr=", &mp.mos.waitsemacount, " sem=", mp.mos.waitsemacount, " ret=", ret, "\n")
		})
	}
}
