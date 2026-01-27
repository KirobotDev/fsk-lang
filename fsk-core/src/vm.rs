use std::collections::HashMap;

#[repr(u8)]
#[derive(Debug, Clone, Copy)]
pub enum OpCode {
    LoadConst = 0,
    Add = 1,
    Sub = 2,
    Mult = 3,
    Div = 4,
    Print = 5,
    Halt = 255,
}

pub struct VM {
    registers: [f64; 256],
    constants: Vec<f64>,
    ip: usize,
}

impl VM {
    pub fn new() -> Self {
        Self {
            registers: [0.0; 256],
            constants: Vec::new(),
            ip: 0,
        }
    }

    pub fn run(&mut self, bytecode: &[u8], constants: Vec<f64>) {
        self.constants = constants;
        self.ip = 0;

        while self.ip < bytecode.len() {
            let opcode = bytecode[self.ip];
            self.ip += 1;

            match opcode {
                0 => { 
                    let dest = bytecode[self.ip] as usize;
                    let idx = bytecode[self.ip + 1] as usize;
                    self.registers[dest] = self.constants[idx];
                    self.ip += 2;
                }
                1 => { 
                    let dest = bytecode[self.ip] as usize;
                    let r1 = bytecode[self.ip + 1] as usize;
                    let r2 = bytecode[self.ip + 2] as usize;
                    self.registers[dest] = self.registers[r1] + self.registers[r2];
                    self.ip += 3;
                }
                5 => { 
                    let r1 = bytecode[self.ip] as usize;
                    println!("[VM] {}", self.registers[r1]);
                    self.ip += 1;
                }
                255 => break,
                _ => {
                    println!("[VM] Unknown opcode: {}", opcode);
                    break;
                }
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn fsk_vm_run(bytecode_ptr: *const u8, bytecode_len: usize, constants_ptr: *const f64, constants_len: usize) {
    let bytecode = unsafe { std::slice::from_raw_parts(bytecode_ptr, bytecode_len) };
    let constants = unsafe { std::slice::from_raw_parts(constants_ptr, constants_len) }.to_vec();

    let mut vm = VM::new();
    vm.run(bytecode, constants);
}
