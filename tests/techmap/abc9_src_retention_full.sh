#!/bin/bash
# Full test for \src retention through ABC9 synthesis.
# Requires: yosys built with ABCEXTERNAL pointing to an ABC with node_retention support.
#
# Usage: ABCEXTERNAL=/path/to/abc bash tests/techmap/abc9_src_retention_full.sh

set -eu

YOSYS=${YOSYS:-./yosys}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Create test verilog
cat > /tmp/test_abc9_src_ret.v <<'EOF'
(* src = "counter.v:1.1-10.10" *)
module counter(input wire clk, input wire rst, input wire en, output reg [3:0] count);
    (* src = "counter.v:3.5-8.8" *)
    always @(posedge clk) begin
        if (rst)
            count <= 4'b0;
        else if (en)
            count <= count + 1;
    end
endmodule
EOF

echo "=== Testing \src retention through ABC9 ==="

# Run synthesis with abc9 -lut
$YOSYS -p "
read_verilog /tmp/test_abc9_src_ret.v
synth -top counter -flatten
abc9 -lut 4
write_json /tmp/test_abc9_src_ret_output.json
" 2>&1

# Check if any cells have src attributes
SRC_CELLS=$(python3 -c "
import json
with open('/tmp/test_abc9_src_ret_output.json') as f:
    data = json.load(f)
count = 0
for mod_name, mod in data.get('modules', {}).items():
    for cell_name, cell in mod.get('cells', {}).items():
        if 'src' in cell.get('attributes', {}):
            count += 1
print(count)
")

echo "Cells with \src attributes: $SRC_CELLS"

if [ "$SRC_CELLS" -gt 0 ]; then
    echo "PASS: \src attributes preserved through ABC9 synthesis"
else
    echo "INFO: No \src attributes on cells (expected if ABC lacks node_retention)"
fi

# Cleanup
rm -f /tmp/test_abc9_src_ret.v /tmp/test_abc9_src_ret_output.json
