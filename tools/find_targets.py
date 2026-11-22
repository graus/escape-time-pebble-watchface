#!/usr/bin/env python3
"""Find Misiurewicz points: dive targets with detail at every depth.

    python3 tools/find_targets.py [max_preperiod+period]

A Misiurewicz point M(k, p) is a c whose critical orbit 0, c, c^2 + c, ...
lands, after exactly k steps, on a cycle of exactly period p. These points
lie on the boundary of the Mandelbrot set and the set is asymptotically
self-similar around them (Tan Lei): zooming in on one shows spirals and
branches at every depth, never a flat interior. That is what an hour-long
dive needs, so every target is one.

Newton's method on g(c) = f^(k+p)(0) - f^k(0) from a grid of starting
points finds the roots; each is then checked to have exactly preperiod k and
period p (roots with a smaller k or p, or periodic centers, are dropped).
Prints the points found, with their (k, p) and angle around the set.
"""

import cmath
import sys


def orbit(c, n):
    z, out = 0j, [0j]
    for _ in range(n):
        z = z * z + c
        out.append(z)
    return out


def newton(c, k, p, steps=60):
    for _ in range(steps):
        z, dz = 0j, 0j
        zk, dzk = None, None
        for i in range(1, k + p + 1):
            dz = 2 * z * dz + 1
            z = z * z + c
            if abs(z) > 1e6:
                return None
            if i == k:
                zk, dzk = z, dz
        g, dg = z - zk, dz - dzk
        if dg == 0:
            return None
        step = g / dg
        c -= step
        if abs(step) < 1e-15:
            return c
    return None


def exact_kp(c, k, p, tol=1e-9):
    """True if c is preperiodic with preperiod exactly k and period exactly p."""
    z = orbit(c, k + 2 * p + 2)
    if abs(z[k + p] - z[k]) > tol:
        return False
    if k > 0 and abs(z[k - 1 + p] - z[k - 1]) < tol:
        return False  # preperiod smaller than k (or periodic: a center)
    for q in range(1, p):
        if p % q == 0 and abs(z[k + q] - z[k]) < tol:
            return False  # period smaller than p
    # Preperiod 1 is impossible: z1 = c on a cycle would put 0 on it too.
    return k >= 2


def find(max_total):
    found = {}
    for total in range(3, max_total + 1):
        for p in range(1, total):
            k = total - p
            if k < 2:
                continue
            for gx in range(-44, 12):
                for gy in range(0, 26):
                    c0 = complex(gx * 0.05, gy * 0.05)
                    c = newton(c0, k, p)
                    if c is None or abs(c) > 2.01 or not exact_kp(c, k, p):
                        continue
                    key = (round(c.real, 9), round(abs(c.imag), 9))
                    found.setdefault(key, (k, p, c))
    return sorted(found.values(), key=lambda t: (t[0] + t[1], cmath.phase(t[2] + 0.25)))


def main():
    max_total = int(sys.argv[1]) if len(sys.argv) > 1 else 6
    for k, p, c in find(max_total):
        print("M(%d,%d)  %+.15f %+.15fi   angle %6.1f" %
              (k, p, c.real, c.imag, cmath.phase(c + 0.25) * 180 / cmath.pi))


if __name__ == "__main__":
    main()
