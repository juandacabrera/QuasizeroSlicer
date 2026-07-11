#!/usr/bin/env python3
"""Quasizero Slicer — independent QZmini G-code verifier.

Checks exported G-code against the QZmini safety and correctness rules:
no unintended heating, G28 preserved, no thermal waits, refill markers,
plunger reset before pause, no physical restore of plunger depth after
refill, logical E consistency, safe return motion, and volume agreement.
Implemented independently from the C++ engine (different parser) so it
cross-checks rather than mirrors it. GNU AGPLv3.
"""
import re, sys, math

BARREL_AREA = math.pi * 17.5 ** 2
MM_PER_E = 0.277
MM3_PER_E = BARREL_AREA * MM_PER_E

def verify(path, expected_refills=None, expected_ml=None, tol_ml=1.0):
    errors = []
    lines = open(path).read().splitlines()
    e_rel = False
    logical_e = 0.0
    physical_e = 0.0            # net plunger advance
    deposited_e = 0.0
    pending_retract = 0.0
    in_refill = False
    refills = 0
    saw_g28 = False
    resets_before_pause = []
    seq_lines = []

    for ln in lines:
        code = ln.split(';', 1)[0].strip()
        comment = ln.split(';', 1)[1] if ';' in ln else ''
        if 'QZ_REFILL_BEGIN' in comment: in_refill = True; refills += 1; seq_lines = []
        if in_refill: seq_lines.append(ln)
        if 'QZ_REFILL_END' in comment:
            in_refill = False
            txt = '\n'.join(seq_lines)
            m400 = txt.find('M400'); reset = txt.find('G1 E-'); pause = re.search(r'\n(M0|M25|M226|M600|M601|PAUSE|M400 U1)', txt)
            if reset == -1: errors.append('refill %d: no plunger reset before pause' % refills)
            elif pause and reset > pause.start(): errors.append('refill %d: reset after pause' % refills)
            if m400 == -1: errors.append('refill %d: missing M400' % refills)
            if 'G92 E' not in txt: errors.append('refill %d: logical E not restored via G92' % refills)
            # no physical restore: after the pause, no positive E move equal to the reset amount
            mr = re.search(r'G1 E-([0-9.]+)', txt)
            if mr and pause:
                after = txt[pause.end():]
                if re.search(r'G1 E' + re.escape(mr.group(1)), after):
                    errors.append('refill %d: plunger physically restored after refill' % refills)
            # return motion: XY return must precede Z lower
            xy = txt.find('; return above print'); zl = txt.find('; lower to printing height')
            if xy == -1 or zl == -1 or xy > zl: errors.append('refill %d: unsafe return motion order' % refills)
        if not code:
            continue
        # heating / thermal waits are forbidden anywhere (S>0)
        m = re.match(r'(M104|M109|M140|M190|M191)\b', code)
        if m:
            sv = re.search(r'S(-?\d+\.?\d*)', code)
            if sv and float(sv.group(1)) > 0:
                errors.append('unintended heating: ' + code)
            if m.group(1) in ('M109', 'M190', 'M191'):
                sv2 = re.search(r'[SR](-?\d+\.?\d*)', code)
                if sv2 and float(sv2.group(1)) > 0:
                    errors.append('thermal wait: ' + code)
        if code.startswith('G28'): saw_g28 = True
        if code.startswith('M83'): e_rel = True
        if code.startswith('M82'): e_rel = False
        if code.startswith('G90'): e_rel = False
        if code.startswith('G91'): e_rel = True
        g92 = re.match(r'G92\b', code)
        if g92:
            ev = re.search(r'E(-?\d+\.?\d*)', code)
            if ev: logical_e = float(ev.group(1))
            continue
        if re.match(r'G[0123]\b', code):
            ev = re.search(r'E(-?\d+\.?\d*)', code)
            if ev:
                e = float(ev.group(1))
                de = e if e_rel else e - logical_e
                logical_e = logical_e + e if e_rel else e
                physical_e += de
                if in_refill:
                    # QZ semantics: E- is the plunger reset (fresh syringe follows,
                    # not an unretract); E+ is the post-refill prime, which is
                    # consumed material. Normal retract pairing does not apply.
                    if de > 0: deposited_e += de
                    pending_retract = 0.0
                elif de < 0: pending_retract += -de
                elif de > 0:
                    un = min(pending_retract, de); pending_retract -= un
                    deposited_e += de - un
    ml = deposited_e * MM3_PER_E / 1000.0
    if not saw_g28: errors.append('G28 missing')
    if expected_refills is not None and refills != expected_refills:
        errors.append('expected %d refills, found %d' % (expected_refills, refills))
    if expected_ml is not None and abs(ml - expected_ml) > tol_ml:
        errors.append('deposited volume %.2f ml, expected %.2f ± %.1f' % (ml, expected_ml, tol_ml))
    status = 'PASS' if not errors else 'FAIL'
    print('%s %s  (refills=%d, deposited=%.2f ml)' % (status, path, refills, ml))
    for e in errors: print('   -', e)
    return not errors

if __name__ == '__main__':
    ok = True
    ok &= verify('samples/qzmini/qz_sample_calibration_line.gcode', 0, 0.24, 0.05)
    ok &= verify('samples/qzmini/qz_sample_no_refill.gcode', 0, 100.0)
    # deposited excludes the post-refill prime? prime IS deposited: 1 ml per refill
    ok &= verify('samples/qzmini/qz_sample_one_refill.gcode', 1, 131.0)
    ok &= verify('samples/qzmini/qz_sample_two_refills.gcode', 2, 262.0)
    sys.exit(0 if ok else 1)
