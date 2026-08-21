`define Mem_Member_Definition(__class, __prefix)\
    __class __prefix``_bits;  \
    ctrl_t __prefix``_valid;      \
    ctrl_t __prefix``_ready;

`define Master_Modport_Declaration(__prefix)\
    output __prefix``_bits,   \
    output __prefix``_valid,  \
    input  __prefix``_ready

`define Slave_Modport_Declaration(__prefix)\
    input __prefix``_bits,   \
    input __prefix``_valid,  \
    output __prefix``_ready