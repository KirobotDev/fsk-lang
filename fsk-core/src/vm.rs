use std::collections::HashMap;
use serde::{Deserialize, Serialize};

#[repr(u8)]
#[derive(Debug, Clone, Copy)]
pub enum OpCode {
    LoadConst = 0,
    Add = 1,
    Sub = 2,
    Mult = 3,
    Div = 4,
    Print = 5,
    Jump = 6,
    JumpIfFalse = 7,
    Equal = 8,
    Greater = 9,
    Less = 10,
    Move = 11,
    CreateObj = 12,
    SetProp = 13,
    GetProp = 14,
    CreateArr = 15,
    PushArr = 16,
    Call = 17,
    Return = 18,
    LoadGlobal = 19,
    StoreGlobal = 20,
    Halt = 255,
}

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(untagged)]
pub enum VMValue {
    #[serde(rename = "Function")]
    Function {
        bytecode_idx: usize,
        arity: u8,
        #[serde(default)]
        is_func: bool,
    },
    Number(f64),
    String(String),
    Bool(bool),
    Array(Vec<VMValue>),
    Object(HashMap<String, VMValue>),
    Nil,
}

impl VMValue {
    pub fn as_f64(&self) -> f64 {
        match self {
            VMValue::Number(n) => *n,
            VMValue::Bool(true) => 1.0,
            _ => 0.0,
        }
    }
}

#[derive(Debug)]
struct CallFrame {
    ip: usize,
    base_reg: usize,
    return_reg: usize,
}

pub struct VM {
    registers: Vec<VMValue>,
    globals: HashMap<String, VMValue>,
    constants: Vec<VMValue>,
    frames: Vec<CallFrame>,
    ip: usize,
    base_reg: usize,
}

impl VM {
    pub fn new() -> Self {
        Self {
            registers: vec![VMValue::Nil; 4096],
            globals: HashMap::new(),
            constants: Vec::new(),
            frames: Vec::new(),
            ip: 0,
            base_reg: 0,
        }
    }

    pub fn run(&mut self, bytecode: &[u8], constants: Vec<VMValue>) {
        self.constants = constants;
        self.ip = 0;
        self.base_reg = 0;

        while self.ip < bytecode.len() {
            let opcode = bytecode[self.ip];
            self.ip += 1;

            match opcode {
                0 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    let idx = bytecode[self.ip + 1] as usize;
                    self.registers[dest] = self.constants[idx].clone();
                    self.ip += 2;
                }
                1 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    let r1 = self.base_reg + bytecode[self.ip + 1] as usize;
                    let r2 = self.base_reg + bytecode[self.ip + 2] as usize;
                    if let (VMValue::Number(n1), VMValue::Number(n2)) = (&self.registers[r1], &self.registers[r2]) {
                        self.registers[dest] = VMValue::Number(n1 + n2);
                    }
                    self.ip += 3;
                }
                2 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    let r1 = self.base_reg + bytecode[self.ip + 1] as usize;
                    let r2 = self.base_reg + bytecode[self.ip + 2] as usize;
                    if let (VMValue::Number(n1), VMValue::Number(n2)) = (&self.registers[r1], &self.registers[r2]) {
                        self.registers[dest] = VMValue::Number(n1 - n2);
                    }
                    self.ip += 3;
                }
                3 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    let r1 = self.base_reg + bytecode[self.ip + 1] as usize;
                    let r2 = self.base_reg + bytecode[self.ip + 2] as usize;
                    if let (VMValue::Number(n1), VMValue::Number(n2)) = (&self.registers[r1], &self.registers[r2]) {
                        self.registers[dest] = VMValue::Number(n1 * n2);
                    }
                    self.ip += 3;
                }
                4 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    let r1 = self.base_reg + bytecode[self.ip + 1] as usize;
                    let r2 = self.base_reg + bytecode[self.ip + 2] as usize;
                    if let (VMValue::Number(n1), VMValue::Number(n2)) = (&self.registers[r1], &self.registers[r2]) {
                        self.registers[dest] = VMValue::Number(n1 / n2);
                    }
                    self.ip += 3;
                }
                5 => { 
                    let r1 = self.base_reg + bytecode[self.ip] as usize;
                    match &self.registers[r1] {
                        VMValue::Number(n) => println!("[VM] {}", n),
                        VMValue::String(s) => println!("[VM] {}", s),
                        VMValue::Bool(b) => println!("[VM] {}", b),
                        VMValue::Array(a) => println!("[VM] {:?}", a),
                        VMValue::Object(m) => println!("[VM] {:?}", m),
                        VMValue::Function { .. } => println!("[VM] <function>"),
                        VMValue::Nil => println!("[VM] nil"),
                    }
                    self.ip += 1;
                }
                6 => { 
                    let offset = bytecode[self.ip] as usize;
                    self.ip = offset;
                }
                7 => { 
                    let r1 = self.base_reg + bytecode[self.ip] as usize;
                    let offset = bytecode[self.ip + 1] as usize;
                    let condition = match &self.registers[r1] {
                        VMValue::Bool(b) => *b,
                        VMValue::Number(n) => *n != 0.0,
                        VMValue::Nil => false,
                        _ => true,
                    };
                    if !condition {
                        self.ip = offset;
                    } else {
                        self.ip += 2;
                    }
                }
                8 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    let r1 = self.base_reg + bytecode[self.ip + 1] as usize;
                    let r2 = self.base_reg + bytecode[self.ip + 2] as usize;
                    self.registers[dest] = VMValue::Bool(self.registers[r1] == self.registers[r2]);
                    self.ip += 3;
                }
                9 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    let r1 = self.base_reg + bytecode[self.ip + 1] as usize;
                    let r2 = self.base_reg + bytecode[self.ip + 2] as usize;
                    if let (VMValue::Number(n1), VMValue::Number(n2)) = (&self.registers[r1], &self.registers[r2]) {
                        self.registers[dest] = VMValue::Bool(n1 > n2);
                    }
                    self.ip += 3;
                }
                10 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    let r1 = self.base_reg + bytecode[self.ip + 1] as usize;
                    let r2 = self.base_reg + bytecode[self.ip + 2] as usize;
                    if let (VMValue::Number(n1), VMValue::Number(n2)) = (&self.registers[r1], &self.registers[r2]) {
                        self.registers[dest] = VMValue::Bool(n1 < n2);
                    }
                    self.ip += 3;
                }
                11 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    let src = self.base_reg + bytecode[self.ip + 1] as usize;
                    self.registers[dest] = self.registers[src].clone();
                    self.ip += 2;
                }
                12 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    self.registers[dest] = VMValue::Object(HashMap::new());
                    self.ip += 1;
                }
                13 => { 
                    let obj_reg = self.base_reg + bytecode[self.ip] as usize;
                    let key_idx = bytecode[self.ip + 1] as usize;
                    let val_reg = self.base_reg + bytecode[self.ip + 2] as usize;
                    if let Some(VMValue::String(key)) = self.constants.get(key_idx) {
                        let value = self.registers[val_reg].clone();
                        let key_str = key.clone();
                        if let VMValue::Object(ref mut map) = self.registers[obj_reg] {
                            map.insert(key_str, value);
                        }
                    }
                    self.ip += 3;
                }
                14 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    let obj_reg = self.base_reg + bytecode[self.ip + 1] as usize;
                    let key_idx = bytecode[self.ip + 2] as usize;
                    if let Some(VMValue::String(key)) = self.constants.get(key_idx) {
                        if let VMValue::Object(map) = &self.registers[obj_reg] {
                            self.registers[dest] = map.get(key).cloned().unwrap_or(VMValue::Nil);
                        }
                    }
                    self.ip += 3;
                }
                15 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    self.registers[dest] = VMValue::Array(Vec::new());
                    self.ip += 1;
                }
                16 => { 
                    let arr_reg = self.base_reg + bytecode[self.ip] as usize;
                    let val_reg = self.base_reg + bytecode[self.ip + 1] as usize;
                    let value = self.registers[val_reg].clone();
                    if let VMValue::Array(ref mut arr) = self.registers[arr_reg] {
                        arr.push(value);
                    }
                    self.ip += 2;
                }
                17 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    let func_reg = self.base_reg + bytecode[self.ip + 1] as usize;
                    let arg_count = bytecode[self.ip + 2] as usize;
                    
                    if let VMValue::Function { bytecode_idx, arity, .. } = self.registers[func_reg] {
                        if (arity as usize) != arg_count {
                            println!("[VM ERROR] Arity mismatch: expected {}, got {}", arity, arg_count);
                            break;
                        }
                        
                        self.frames.push(CallFrame {
                            ip: self.ip + 3,
                            base_reg: self.base_reg,
                            return_reg: dest,
                        });
                        
                        self.base_reg = func_reg + 1;
                        self.ip = bytecode_idx;
                    } else {
                        println!("[VM ERROR] Trying to call non-function at register {}: {:?}", func_reg, self.registers[func_reg]);
                        break;
                    }
                }
                18 => { 
                    let val_reg = self.base_reg + bytecode[self.ip] as usize;
                    let return_val = self.registers[val_reg].clone();
                    
                    if let Some(frame) = self.frames.pop() {
                        self.ip = frame.ip;
                        self.base_reg = frame.base_reg;
                        self.registers[frame.return_reg] = return_val;
                    } else {
                        break;
                    }
                }
                19 => { 
                    let dest = self.base_reg + bytecode[self.ip] as usize;
                    let name_idx = bytecode[self.ip + 1] as usize;
                    if let Some(VMValue::String(name)) = self.constants.get(name_idx) {
                        self.registers[dest] = self.globals.get(name).cloned().unwrap_or(VMValue::Nil);
                    }
                    self.ip += 2;
                }
                20 => { 
                    let name_idx = bytecode[self.ip] as usize;
                    let src = self.base_reg + bytecode[self.ip + 1] as usize;
                    if let Some(VMValue::String(name)) = self.constants.get(name_idx) {
                        self.globals.insert(name.clone(), self.registers[src].clone());
                    }
                    self.ip += 2;
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
pub extern "C" fn fsk_vm_run(bytecode_ptr: *const u8, bytecode_len: usize, constants_json: *const std::os::raw::c_char) {
    let bytecode = unsafe { std::slice::from_raw_parts(bytecode_ptr, bytecode_len) };
    let json_str = unsafe { std::ffi::CStr::from_ptr(constants_json) }.to_string_lossy();
    
    let constants: Vec<VMValue> = serde_json::from_str(&json_str).unwrap_or_else(|e| {
        println!("[VM ERROR] Failed to parse constants: {}", e);
        Vec::new()
    });

    let mut vm = VM::new();
    vm.run(bytecode, constants);
}
