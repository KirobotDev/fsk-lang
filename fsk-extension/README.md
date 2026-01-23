# Fsk Language Support for VS Code

This extension adds rich syntax highlighting and language support for **Fsk**, the Divine Intellect Operating Language.

## Features

- 🎨 **Syntax Highlighting**: Complete colorization for keywords, strings, numbers, comments, and functions.
- 🔧 **Snippets**: Common code patterns for `fn`, `class`, `if`, `while`, etc. (Coming Soon)
- 🚀 **File Association**: Automatically detects `.fsk` files.

## Installation

### Method 1: Manual Installation (Developer Mode)

1.  Close VS Code / Antigravity.
2.  Copy this folder (`fsk-extension`) to your extensions directory:

    **Standard VS Code:**
    ```bash
    cp -r . ~/.vscode/extensions/fsk-language-support
    ```
    
    **Antigravity / VS Code OSS:**
    ```bash
    cp -r . ~/.antigravity/extensions/fsk-language-support
    # OR
    cp -r . ~/.vscode-oss-dev/extensions/fsk-language-support
    ```s

3.  Restart your editor.
4.  Open any `.fsk` file and enjoy!

### Method 2: VSIX Installation

If you have the compiled `.vsix` file:

1.  Open VS Code.
2.  Go to **Extensions** (Ctrl+Shift+X).
3.  Click the `...` menu (Views and More Actions).
4.  Select **Install from VSIX...**.
5.  Choose `fsk-lang-1.0.0.vsix`.

## Contributing

This extension is part of the [Fsk Language Project](https://github.com/KirobotDev/fsk-lang).
