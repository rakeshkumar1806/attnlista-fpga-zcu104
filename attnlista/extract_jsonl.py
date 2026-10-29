import json
import re

hw_nmses = []
with open(r'C:\Users\jbvrr\.gemini\antigravity\brain\aeeb61f1-2bf1-4a74-951d-9538e3a22362\.system_generated\logs\transcript_full.jsonl', 'r', encoding='utf-8') as f:
    for line in f:
        try:
            data = json.loads(line)
            if 'content' in data:
                matches = re.findall(r'Test\s+\d+:\s+NMSE\s+=\s+([-0-9.]+)', data['content'])
                if len(matches) == 1000:
                    hw_nmses = [float(x) for x in matches]
        except Exception:
            pass

if len(hw_nmses) == 1000:
    print(f"Found exactly 1000 HW NMSEs. First: {hw_nmses[0]}, Last: {hw_nmses[-1]}")
    with open('hw_nmse_1000.txt', 'w') as out:
        for val in hw_nmses:
            out.write(f"{val}\n")
else:
    print(f"Found {len(hw_nmses)} matches instead of 1000.")
