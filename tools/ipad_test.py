"""Test op de echte iPad (via USB, met de bestaande iPadInstaller-code).

  py -3.12 tools/ipad_test.py install <ipa>     # installeren
  py -3.12 tools/ipad_test.py arm               # autotest klaarzetten (Documents/autotest/go.txt)
  py -3.12 tools/ipad_test.py log <seconden>    # systeemlog meelezen (SGB-regels) terwijl de app draait
  py -3.12 tools/ipad_test.py fetch             # rapport + screenshots ophalen -> device_results/
"""
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(os.path.dirname(ROOT), "iPadInstaller"))
import ipad_installer as ii  # noqa: E402

BUNDLE = "nl.yf.smallgiveawaybike1"
SHOTS = ["01_menu", "02_spawn", "03_walk", "04_jump", "05_bike", "06_car", "07_mission_start",
         "08_mission_cp", "09_mission_passed", "10_quality_low", "11_quality_medium",
         "12_quality_high", "13_night", "14_police", "15_missions", "16_mission_select_start", "17_nl_area", "18_nl_drive"]


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "fetch"
    if cmd == "install":
        ii.install_ipa(sys.argv[2])
    elif cmd == "arm":
        d = tempfile.mkdtemp()
        with open(os.path.join(d, "go.txt"), "w") as f:
            f.write("autotest\n")
        ii.upload_app_documents(BUNDLE, d, "autotest")
        print("autotest klaargezet: open nu de app op de iPad")
    elif cmd == "log":
        secs = float(sys.argv[2]) if len(sys.argv) > 2 else 90
        out = os.path.join(ROOT, "device_results")
        os.makedirs(out, exist_ok=True)
        lines = ii.capture_syslog(secs, match=["SGB", "SmallGiveawayBike1"])
        with open(os.path.join(out, "syslog.txt"), "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
        for l in lines:
            print(l)
    elif cmd == "fetch":
        out = os.path.join(ROOT, "device_results")
        os.makedirs(out, exist_ok=True)
        rep = ii.read_app_document(BUNDLE, "autotest_report.txt")
        if rep:
            open(os.path.join(out, "autotest_report.txt"), "wb").write(rep)
            print(rep.decode("utf-8", "replace"))
        else:
            print("geen rapport gevonden")
        n = 0
        for s in SHOTS:
            data = None
            for attempt in range(4):
                try:
                    data = ii.read_app_document(BUNDLE, "shots/%s.png" % s)
                    break
                except Exception as e:  # USB/SSL hapert soms bij snel achter elkaar verbinden
                    print("opnieuw (%s): %s" % (s, e))
                    import time
                    time.sleep(1.5)
            if data:
                open(os.path.join(out, s + ".png"), "wb").write(data)
                n += 1
        print("%d screenshots opgehaald naar %s" % (n, out))


if __name__ == "__main__":
    main()
