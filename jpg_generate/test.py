import matplotlib.pyplot as plt
import re

# 사용자가 지정한 파일 경로
file_path = '/home/hong/scarab/results/Baseline/leela_17_r_ref/leela_17_r_ref0/leela_17_r_ref0_ckpt2/retired_op_per_cycle.out'

try:
    with open(file_path, 'r') as file:
        log_data = file.read()
except FileNotFoundError:
    print(f"Error: The file '{file_path}' was not found.")
    exit()

# --- 사이클 구간 설정 ---
start_cycle = 1042000
end_cycle = 1042500
# -------------------------

# (이전과 동일한 데이터 파싱 및 필터링 로직)
all_ops_data = {}
on_path_mispred_cycles = []
off_path_mispred_cycles = []
all_recovery_cycles = []
for line in log_data.strip().split('\n'):
    if line.startswith("Cycle:"):
        match = re.search(r"Cycle:(\d+)\s+Retired Ops: (\d+)", line)
        if match:
            cycle, ops = int(match.group(1)), int(match.group(2))
            all_ops_data[cycle] = ops
    if "[Mispred for PC" in line:
        match = re.search(r"\[Cycle\s*(\d+)\s*\]", line)
        if match:
            cycle = int(match.group(1))
            if "(off_path:0)" in line: on_path_mispred_cycles.append(cycle)
            elif "(off_path:1)" in line: off_path_mispred_cycles.append(cycle)
    if "[Recovery End" in line:
        match = re.search(r"\[Cycle\s*(\d+)\s*\]", line)
        if match: all_recovery_cycles.append(int(match.group(1)))
plot_cycles = list(range(start_cycle, end_cycle + 1))
plot_ops = [all_ops_data.get(cycle, 0) for cycle in plot_cycles]
on_path_mispred_filtered = [c for c in on_path_mispred_cycles if start_cycle <= c <= end_cycle]
off_path_mispred_filtered = [c for c in off_path_mispred_cycles if start_cycle <= c <= end_cycle]
recovery_filtered = [c for c in all_recovery_cycles if start_cycle <= c <= end_cycle]


# --- 그래프 생성 ---
plt.figure(figsize=(15, 7))
plt.plot(plot_cycles, plot_ops, linestyle='-', label='Retired Ops')
for i, c in enumerate(on_path_mispred_filtered): plt.axvline(x=c, color='orange', ls='--', lw=1, label='On-path Mispred' if i == 0 else "")
for i, c in enumerate(off_path_mispred_filtered): plt.axvline(x=c, color='red', ls='--', lw=1, label='Off-path Mispred' if i == 0 else "")
for i, c in enumerate(recovery_filtered): plt.axvline(x=c, color='green', ls=':', lw=1, label='Recovery End' if i == 0 else "")

# --- 그래프 꾸미기 ---
plt.title(f'Retired Ops and Events from Cycle {start_cycle} to {end_cycle}')
plt.xlabel('Cycle')
plt.ylabel('Number of Retired Ops')
plt.grid(True, which='both', linestyle='-', linewidth=0.5)
plt.legend()
plt.tight_layout(rect=[0, 0.05, 1, 1]) # 텍스트 공간 확보를 위해 레이아웃 조정

# --- 파일 경로를 그래프 하단에 추가 ---
plt.figtext(0.5, 0.01, file_path, wrap=True, ha="center", fontsize=8, color="gray")
# ------------------------------------

# 파일로 저장
plt.savefig('retired_ops_final_graph.png', dpi=150)
print("Graph with file path has been saved as retired_ops_final_graph.png")