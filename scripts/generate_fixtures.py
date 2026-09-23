import math
import numpy as np
import os

def generate_fixtures():
    fixtures_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "tests", "fixtures")
    os.makedirs(fixtures_dir, exist_ok=True)

    # -------------------------------------------------------------------------
    # 1. polyblep_saw_golden.csv
    # 100 samples of isolated 4th-order PolyBLEP saw wave at 440 Hz (fs=44100)
    # -------------------------------------------------------------------------
    fs_saw = np.float32(44100.0)
    f0_saw = np.float32(440.0)
    dt_saw = f0_saw / fs_saw

    def polyblep4(t, dt):
        c = np.float32(0.095)
        a1 = np.float32(2.0 - 8.0 * 0.095)    # 1.24
        a3 = np.float32(16.0 * 0.095 - 2.0)   # -0.48
        a4 = np.float32(1.0 - 9.0 * 0.095)    # 0.145

        if t < dt:
            x = t / dt
            return np.float32(-1.0 + x * (a1 + x * x * (a3 + a4 * x)))
        if t < np.float32(2.0) * dt:
            x = t / dt
            u = np.float32(2.0) - x
            u2 = u * u
            return np.float32(-c * (u2 * u2))
        if t > np.float32(1.0) - dt:
            x = (t - np.float32(1.0)) / dt
            return np.float32(1.0 + x * (a1 + x * x * (a3 - a4 * x)))
        if t > np.float32(1.0) - np.float32(2.0) * dt:
            x = (t - np.float32(1.0)) / dt
            u = x + np.float32(2.0)
            u2 = u * u
            return np.float32(c * (u2 * u2))
        return np.float32(0.0)

    def wrap01(phase):
        p = phase - np.float32(math.floor(float(phase)))
        if p >= np.float32(1.0) or p < np.float32(0.0):
            p = np.float32(0.0)
        return p

    saw_file = os.path.join(fixtures_dir, "polyblep_saw_golden.csv")
    with open(saw_file, "w", encoding="utf-8") as f:
        f.write("# Bumbler XD Algorithm Golden Fixture: 4th-Order PolyBLEP Saw Wave\n")
        f.write("# Frequency: 440.0 Hz, SampleRate: 44100.0 Hz, Samples: 100\n")
        f.write("# Spec: docs/DSP_ALGORITHMS.md Sections 2.2 and 2.3 (Eq 2.2.1-2.2.6, 2.3.1-2.3.2)\n")
        f.write("sample_idx,phase,naive_saw,polyblep_residual,output\n")

        phase = np.float32(0.0)
        for i in range(100):
            t = wrap01(phase)
            naive = np.float32(np.float32(2.0) * t - np.float32(1.0))
            residual = polyblep4(t, dt_saw)
            out = naive - residual
            f.write(f"{i},{t:.8e},{naive:.8e},{residual:.8e},{out:.8e}\n")
            phase = wrap01(phase + dt_saw)

    print(f"Generated: {saw_file}")

    # -------------------------------------------------------------------------
    # 2. polyblep_pulse_golden.csv
    # 100 samples of isolated 4th-order PolyBLEP square/pulse wave at 440 Hz
    # Evaluated at PW=0.50 and PW=0.25 (fs=44100)
    # -------------------------------------------------------------------------
    pulse_file = os.path.join(fixtures_dir, "polyblep_pulse_golden.csv")
    with open(pulse_file, "w", encoding="utf-8") as f:
        f.write("# Bumbler XD Algorithm Golden Fixture: 4th-Order PolyBLEP Pulse Wave\n")
        f.write("# Frequency: 440.0 Hz, SampleRate: 44100.0 Hz, PW1: 0.50, PW2: 0.25, Samples: 100\n")
        f.write("# Spec: docs/DSP_ALGORITHMS.md Section 2.3 (Eq 2.3.3-2.3.4)\n")
        f.write("sample_idx,phase,naive_pulse_50,polyblep_residual_50,output_50,naive_pulse_25,polyblep_residual_25,output_25\n")

        phase = np.float32(0.0)
        pw50 = np.float32(0.50)
        pw25 = np.float32(0.25)
        for i in range(100):
            t = wrap01(phase)
            # PW = 0.50
            naive50 = np.float32(1.0) if t < pw50 else np.float32(-1.0)
            res50 = polyblep4(t, dt_saw) - polyblep4(wrap01(t - pw50), dt_saw)
            out50 = naive50 + res50

            # PW = 0.25
            naive25 = np.float32(1.0) if t < pw25 else np.float32(-1.0)
            res25 = polyblep4(t, dt_saw) - polyblep4(wrap01(t - pw25), dt_saw)
            out25 = naive25 + res25

            f.write(f"{i},{t:.8e},{naive50:.8e},{res50:.8e},{out50:.8e},{naive25:.8e},{res25:.8e},{out25:.8e}\n")
            phase = wrap01(phase + dt_saw)

    print(f"Generated: {pulse_file}")

    # -------------------------------------------------------------------------
    # 3. zdf_svf_lp12_step_golden.csv
    # 100 samples of isolated ZDF 2-pole Lowpass step response (fc=1000 Hz, R=0.707, input=1.0)
    # -------------------------------------------------------------------------
    kPi_f = float(np.float32(3.14159265358979323846)) # kPi in BumblerCommon.h is float
    fs_filter = 44100.0
    fc = 1000.0
    reso12 = 0.707
    g12 = math.tan(kPi_f * fc / fs_filter)
    k12 = 2.0 - 1.9 * reso12

    class SvfStagePy:
        def __init__(self):
            self.s1 = 0.0
            self.s2 = 0.0

        def step(self, in_val, g, k):
            d = 1.0 + g * (g + k)
            hp = (in_val - (g + k) * self.s1 - self.s2) / d

            v1 = g * hp
            bp = v1 + self.s1
            self.s1 = bp + v1

            v2 = g * bp
            lp = v2 + self.s2
            self.s2 = lp + v2

            notch = hp + lp

            if abs(self.s1) < 1.0e-15:
                self.s1 = 0.0
            if abs(self.s2) < 1.0e-15:
                self.s2 = 0.0

            self.s1 = max(-12.0, min(12.0, self.s1))
            self.s2 = max(-12.0, min(12.0, self.s2))

            return lp, bp, hp, notch

    svf12 = SvfStagePy()
    lp12_file = os.path.join(fixtures_dir, "zdf_svf_lp12_step_golden.csv")
    with open(lp12_file, "w", encoding="utf-8") as f:
        f.write("# Bumbler XD Algorithm Golden Fixture: ZDF SVF LP12 Step Response\n")
        f.write(f"# Cutoff: {fc} Hz, Resonance R: {reso12}, Input: 1.0, SampleRate: {fs_filter} Hz, Samples: 100\n")
        f.write("# Spec: docs/DSP_ALGORITHMS.md Sections 3.1-3.4 (Eq 3.2.1, 3.3.1-3.3.12, Mode 0)\n")
        f.write("sample_idx,input,lp,bp,hp,notch,output\n")

        for i in range(100):
            lp, bp, hp, notch = svf12.step(1.0, g12, k12)
            out = float(np.float32(lp))
            f.write(f"{i},1.00000000e+00,{lp:.8e},{bp:.8e},{hp:.8e},{notch:.8e},{out:.8e}\n")

    print(f"Generated: {lp12_file}")

    # -------------------------------------------------------------------------
    # 4. zdf_svf_lp24_fat_step_golden.csv
    # 100 samples of isolated ZDF 4-pole Lowpass FAT step response with CMOS 4069UB
    # saturation (fc=1000 Hz, resonance=0.75, input=1.0)
    # -------------------------------------------------------------------------
    reso24 = 0.75
    g24 = math.tan(kPi_f * fc / fs_filter)
    k24 = 2.0 - 1.9 * reso24

    svf24_stg1 = SvfStagePy()
    svf24_stg2 = SvfStagePy()

    lp24_file = os.path.join(fixtures_dir, "zdf_svf_lp24_fat_step_golden.csv")
    with open(lp24_file, "w", encoding="utf-8") as f:
        f.write("# Bumbler XD Algorithm Golden Fixture: ZDF SVF LP24 FAT Step Response with CMOS 4069UB Saturation\n")
        f.write(f"# Cutoff: {fc} Hz, Resonance: {reso24}, Input: 1.0, SampleRate: {fs_filter} Hz, Samples: 100\n")
        f.write("# Spec: docs/DSP_ALGORITHMS.md Sections 3.4 (Mode 1) and 3.5 (Eq 3.4.1-3.4.2, 3.5.1-3.5.2)\n")
        f.write("sample_idx,input,stage1_lp,stage1_sat,stage2_lp,output\n")

        for i in range(100):
            lp1, bp1, hp1, nt1 = svf24_stg1.step(1.0, g24, k24)
            stageIn = math.tanh(lp1 * 1.2) / 1.2
            lp2, bp2, hp2, nt2 = svf24_stg2.step(stageIn, g24, k24)
            out = float(np.float32(lp2))
            f.write(f"{i},1.00000000e+00,{lp1:.8e},{stageIn:.8e},{lp2:.8e},{out:.8e}\n")

    print(f"Generated: {lp24_file}")

    # -------------------------------------------------------------------------
    # 5. haas_decorrelation_golden.csv
    # Isolated Haas 5ms delay buffer response for an impulse input (fs=48000, delay=240 samples)
    # -------------------------------------------------------------------------
    fs_haas = 48000.0
    delay_samples = int(0.005 * fs_haas + 0.5) # 240
    kMaxDelaySamples = 4096
    kMask = kMaxDelaySamples - 1
    kInvSqrt2 = 0.7071067811865475

    delay_ring = [0.0] * kMaxDelaySamples
    ring_idx = 0

    haas_file = os.path.join(fixtures_dir, "haas_decorrelation_golden.csv")
    with open(haas_file, "w", encoding="utf-8") as f:
        f.write("# Bumbler XD Algorithm Golden Fixture: Haas 5ms Psychoacoustic Stereo Decorrelator\n")
        f.write(f"# SampleRate: {fs_haas} Hz, Delay: {delay_samples} samples (5.0 ms), Samples: 300\n")
        f.write("# Spec: docs/DSP_ALGORITHMS.md Section 5.3 (Eq 5.3.1-5.3.4)\n")
        f.write("sample_idx,mono_in,delayed,out_left,out_right\n")

        for i in range(300):
            mono_in = 1.0 if i == 0 else 0.0

            delay_ring[ring_idx] = mono_in
            read_idx = (ring_idx - delay_samples + kMaxDelaySamples) & kMask
            delayed = delay_ring[read_idx]
            ring_idx = (ring_idx + 1) & kMask

            diff = mono_in - delayed
            raw_l = (mono_in + 0.6 * diff) * kInvSqrt2
            raw_r = (delayed - 0.6 * diff) * kInvSqrt2

            out_l = float(np.float32(max(-1.05, min(1.05, raw_l))))
            out_r = float(np.float32(max(-1.05, min(1.05, raw_r))))

            f.write(f"{i},{mono_in:.8e},{delayed:.8e},{out_l:.8e},{out_r:.8e}\n")

    print(f"Generated: {haas_file}")

if __name__ == "__main__":
    generate_fixtures()
