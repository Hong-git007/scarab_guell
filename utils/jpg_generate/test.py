import matplotlib.pyplot as plt
import re

# ==================== 설정 변수 ====================

# 1. 분석할 로그 파일의 전체 경로를 지정하세요.
file_path = '../../results/Baseline/leela_17_r_ref/leela_17_r_ref0/leela_17_r_ref0_ckpt3/retired_op_per_cycle.out'

# 2. 그래프로 그릴 사이클 시작/종료 구간을 설정하세요.
start_cycle = 2000000
end_cycle = 2000300  # 로그 파일 전체를 보려면 더 큰 값으로 설정

# ===================================================


try:
    with open(file_path, 'r') as file:
        log_data = file.read()
except FileNotFoundError:
    print(f"오류: '{file_path}' 파일을 찾을 수 없습니다.")
    exit()

# 데이터 파싱을 위한 변수 초기화
all_ops_data = {}
on_path_mispred_cycles = []
off_path_mispred_cycles = []
all_recovery_cycles = []

# 로그 파일 한 줄씩 파싱
for line in log_data.strip().split('\n'):
    # "Cycle:..." 라인에서 Retired Ops 정보 추출
    if line.startswith("Cycle:"):
        match = re.search(r"Cycle:(\d+)\s+Retired Ops: (\d+)", line)
        if match:
            cycle, ops = int(match.group(1)), int(match.group(2))
            all_ops_data[(cycle)] = ops

    # "[Mispred ... Detection for PC" 라인에서 분기 예측 실패 정보 추출
    if "[Mispred" in line and "Detection for PC" in line:
        match = re.search(r"\[Cycle\s*(\d+)\s*\]", line)
        if match:
            cycle = int(match.group(1))
            # on-path와 off-path 구분
            if "(off_path:0)" in line:
                on_path_mispred_cycles.append(cycle)
            elif "(off_path:1)" in line:
                off_path_mispred_cycles.append(cycle)

    # "[Recovery End" 라인에서 복구 완료 정보 추출
    if "[Recovery End" in line:
        match = re.search(r"\[Cycle\s*(\d+)\s*\]", line)
        if match:
            all_recovery_cycles.append(int(match.group(1)))

# 설정된 사이클 구간에 맞게 데이터 필터링
plot_cycles = list(range(start_cycle, end_cycle + 1))
plot_ops = [all_ops_data.get(cycle, 0) for cycle in plot_cycles]
on_path_mispred_filtered = [c for c in on_path_mispred_cycles if start_cycle <= c <= end_cycle]
off_path_mispred_filtered = [c for c in off_path_mispred_cycles if start_cycle <= c <= end_cycle]
recovery_filtered = [c for c in all_recovery_cycles if start_cycle <= c <= end_cycle]

# 그래프 생성
try:
    plt.style.use('seaborn-v0_8-whitegrid')
except OSError:
    try:
        plt.style.use('seaborn-whitegrid')
    except OSError:
        pass  # 스타일을 찾을 수 없으면 기본 스타일 사용

plt.figure(figsize=(18, 8))

# Retired Ops 그래프 그리기
plt.plot(plot_cycles, plot_ops, linestyle='-', marker='.', markersize=3, label='Retired Ops per Cycle', zorder=5)

# 이벤트 발생 지점에 수직선 그리기
for i, c in enumerate(on_path_mispred_filtered):
    plt.axvline(x=c, color='red', linestyle='--', linewidth=1.2, label='Misprediction Detected' if i == 0 else "") # 색상 변경: orange -> red
for i, c in enumerate(off_path_mispred_filtered):
    plt.axvline(x=c, color='red', linestyle='--', linewidth=1.2, label='Off-path Misprediction' if i == 0 else "") # 필요하다면 색상 변경
for i, c in enumerate(recovery_filtered):
    plt.axvline(x=c, color='green', linestyle=':', linewidth=1.5, label='Recovery End' if i == 0 else "")

# 그래프 꾸미기
plt.title(f'Retired Ops and Branch Prediction Events (Cycles: {start_cycle} to {end_cycle})', fontsize=16)
plt.xlabel('Cycle', fontsize=12)
plt.ylabel('Number of Retired Ops', fontsize=12)
plt.legend(loc='upper left')
plt.xlim(start_cycle, end_cycle) # X축 범위 고정
plt.ylim(bottom=0) # Y축 최솟값 0으로 고정

# 그래프 하단에 파일 경로 추가
plt.figtext(0.5, 0.01, f'Data Source: {file_path}', wrap=True, ha="center", fontsize=9, color="black")
plt.tight_layout(rect=[0, 0.05, 1, 0.95]) # 제목과 텍스트 공간 확보

# 파일로 저장 및 완료 메시지 출력
output_filename = 'retired_ops_and_events_graph.png'
plt.savefig(output_filename, dpi=200)
print(f"그래프가 성공적으로 '{output_filename}' 파일로 저장되었습니다.")