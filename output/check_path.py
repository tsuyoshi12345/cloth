import pandas as pd
import sys

# 元のCSVファイルを読み込む
df = pd.read_csv('payments_output.csv')
argv = sys.argv

# 条件に一致する行をフィルタリングする
from_node_id = int(argv[1])
to_node_id = int(argv[2])
filtered_df = df[(df['sender_id'] == from_node_id) & (df['receiver_id'] == to_node_id)]

file_name = argv[3]

# 結果を新しいCSVファイルに書き込む
filtered_df.to_csv(file_name, index=False)
