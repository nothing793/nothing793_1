`include "core_struct.vh"

module DataTrunc (
    input CorePack::data_t dmem_rdata,
    input CorePack::mem_op_enum mem_op,
    input CorePack::addr_t dmem_raddr,
    output CorePack::data_t read_data
);

  logic [2:0]offset;
  logic [7:0]byte_val;
  logic [15:0]half_val;
  logic [31:0]word_val;

  assign offset=dmem_raddr[2:0];

  import CorePack::*;

  // Data trunction
  always_comb begin 
    byte_val = dmem_rdata[offset*8+: 8];
    half_val = dmem_rdata[offset*8+: 16];
    word_val = dmem_rdata[offset*8+: 32];

    case (mem_op)
      MEM_B:read_data = {{56{byte_val[7]}},byte_val};
      MEM_UB:read_data = {56'b0,byte_val};
      MEM_H:read_data = {{48{half_val[15]}},half_val};
      MEM_UH:read_data = {48'b0,half_val};
      MEM_W:read_data = {{32{word_val[31]}},word_val};
      MEM_UW:read_data = {32'b0,word_val};
      MEM_D: read_data = dmem_rdata;
      default: read_data = '0;
    endcase
  end

endmodule
