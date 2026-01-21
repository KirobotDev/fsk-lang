module.exports = {
    apps: [
        {
            name: "fsk-docs",
            script: "./build/fsk",
            args: "docs/server.fsk",
            interpreter: "none",
            cwd: ".",
            exec_mode: "fork",
            env: {
                NODE_ENV: "production",
            }
        },
        {
            name: "fsk-registry",
            script: "server.js",
            cwd: "./registry",
            interpreter: "node",
            env: {
                PORT: 3000,
                NODE_ENV: "production"
            }
        }
    ]
};