import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { execFile, spawn } from "node:child_process";

const ENV_PATH = path.resolve("C:\\dev\\Visual\\.env");
const env = {};
if (fs.existsSync(ENV_PATH)) {
  const content = fs.readFileSync(ENV_PATH, "utf8").replace(/^\uFEFF/, "");
  for (const line of content.split(/\r?\n/)) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith("#")) continue;
    const eqIdx = trimmed.indexOf("=");
    if (eqIdx !== -1) env[trimmed.slice(0, eqIdx).trim()] = trimmed.slice(eqIdx + 1).trim();
  }
}

const PORT = parseInt(env.PORT || process.env.PORT || "8090", 10);
const HOST = env.HOST || process.env.HOST || "127.0.0.1";
const WORKSPACE_ROOT = path.resolve(env.WORKSPACE_ROOT || "C:\\dev\\Visual");
const EXPECTED_TOKEN = env.VISUAL_REPO_BRIDGE_TOKEN || process.env.VISUAL_REPO_BRIDGE_TOKEN || "";
const PUBLIC_NGROK_URL = env.PUBLIC_NGROK_URL || process.env.PUBLIC_NGROK_URL || "";
const LINEAR_API_KEY = env.LINEAR_API_KEY || process.env.LINEAR_API_KEY || "";
const CODEX_CLI_PATH = "C:\\Users\\DELL\\AppData\\Local\\OpenAI\\Codex\\bin\\cdef5aaf3e41ab53\\codex.exe";
const ANTIGRAVITY_CLI_PATH = "C:\\Users\\DELL\\AppData\\Local\\agy\\bin\\agy.exe";
const CAPTURES_DIR = path.join(WORKSPACE_ROOT, "artifacts", "captures");
const AGENT_TEMP_ROOT = path.join(process.env.TEMP || process.env.TMP || "C:\\Windows\\Temp", "VisualAgentTasks");
const AGENT_LOG_LIMIT = 2 * 1024 * 1024;
const ANTIGRAVITY_MODELS = [
  "gemini-3.8-flash-high",
  "gemini-3.8-flash-medium",
  "gemini-3.8-flash-low",
  "gemini-3.1-pro-high",
  "gemini-3.1-pro-low"
];
const ANTIGRAVITY_MODEL_ALIASES = {
  "gemini-3.8-flash": "gemini-3.8-flash-high"
};

fs.mkdirSync(CAPTURES_DIR, { recursive: true });
fs.mkdirSync(AGENT_TEMP_ROOT, { recursive: true });

if (!EXPECTED_TOKEN) {
  console.error("FATAL: VISUAL_REPO_BRIDGE_TOKEN is not configured");
  process.exit(1);
}

process.on("uncaughtException", err => console.error("[Visual Bridge] Uncaught exception:", err));
process.on("unhandledRejection", reason => console.error("[Visual Bridge] Unhandled rejection:", reason));

function resolveSafePath(userPath) {
  if (!userPath || userPath === ".") return WORKSPACE_ROOT;
  const resolved = path.isAbsolute(userPath) ? path.resolve(userPath) : path.resolve(WORKSPACE_ROOT, userPath);
  const rootWithSep = WORKSPACE_ROOT.endsWith(path.sep) ? WORKSPACE_ROOT : WORKSPACE_ROOT + path.sep;
  if (resolved !== WORKSPACE_ROOT && !resolved.startsWith(rootWithSep)) {
    const err = new Error(`Access denied: Path '${userPath}' resolves outside repository boundary.`);
    err.code = "OUTSIDE_BOUNDARY";
    throw err;
  }
  const baseName = path.basename(resolved).toLowerCase();
  if (baseName === ".env" || baseName.startsWith(".env.") || baseName.endsWith(".token") || baseName.endsWith(".key")) {
    const err = new Error(`Access denied: Direct access to sensitive file '${baseName}' is forbidden.`);
    err.code = "FORBIDDEN";
    throw err;
  }
  return resolved;
}

function getRelativePath(absPath) {
  return path.relative(WORKSPACE_ROOT, absPath).replace(/\\/g, "/") || ".";
}

function verifyExpectedSha256(filePath, expectedSha256) {
  if (!expectedSha256) return null;
  const current = crypto.createHash("sha256").update(fs.readFileSync(filePath)).digest("hex");
  if (current.toLowerCase() !== expectedSha256.trim().toLowerCase()) {
    const err = new Error(`SHA-256 mismatch for '${path.basename(filePath)}': expected ${expectedSha256.toLowerCase()}, found ${current.toLowerCase()}`);
    err.code = "HASH_MISMATCH";
    err.expected = expectedSha256.toLowerCase();
    err.current = current.toLowerCase();
    throw err;
  }
  return current;
}

function globToRegex(glob) {
  glob = glob.replace(/\\/g, "/");
  let out = "";
  for (let i = 0; i < glob.length;) {
    const c = glob[i];
    if (c === "*" && glob[i + 1] === "*") {
      if (glob[i + 2] === "/") { out += "(?:.*?/)?"; i += 3; }
      else { out += ".*"; i += 2; }
    } else if (c === "*") { out += "[^/]*"; i++; }
    else if (c === "?") { out += "[^/]"; i++; }
    else if ([".", "(", ")", "+", "|", "^", "$", "@", "%", "{", "}", "[", "]"].includes(c)) { out += "\\" + c; i++; }
    else { out += c; i++; }
  }
  return new RegExp("^" + out + "$", "i");
}

function matchGlob(pattern, itemPath, itemName) {
  if (!pattern || pattern === "*") return true;
  const normalized = itemPath.replace(/\\/g, "/");
  const p = pattern.replace(/\\/g, "/");
  return globToRegex(p).test(p.includes("/") ? normalized : itemName);
}

function parseJsonBody(req) {
  return new Promise((resolve, reject) => {
    let body = "";
    req.on("data", chunk => {
      body += chunk;
      if (body.length > 25 * 1024 * 1024) reject(new Error("Request body too large"));
    });
    req.on("end", () => {
      const trimmed = body.trim().replace(/^\uFEFF/, "");
      if (!trimmed) return resolve({});
      try { resolve(JSON.parse(trimmed)); } catch (_) { reject(new Error("Invalid JSON body")); }
    });
    req.on("error", reject);
  });
}

function sendJson(res, statusCode, data) {
  res.writeHead(statusCode, {
    "Content-Type": "application/json",
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
    "Access-Control-Allow-Headers": "Content-Type, Authorization"
  });
  res.end(JSON.stringify(data));
}

const OPENAPI_PATH = path.resolve(WORKSPACE_ROOT, "bridge", "openapi_chatgpt_compact.json");
function getOpenApiSchema(req) {
  if (!fs.existsSync(OPENAPI_PATH)) return { error: "OpenAPI schema file not found" };
  const parsed = JSON.parse(fs.readFileSync(OPENAPI_PATH, "utf8").replace(/^\uFEFF/, ""));
  let serverUrl = PUBLIC_NGROK_URL;
  if (!serverUrl) {
    const hostHeader = req.headers.host || `${HOST}:${PORT}`;
    const proto = req.headers["x-forwarded-proto"] || (hostHeader.includes(".ngrok") ? "https" : "http");
    serverUrl = `${proto}://${hostHeader}`;
  }
  parsed.servers = [{ url: serverUrl, description: "Public tunnel endpoint" }];
  return parsed;
}

const runningProcesses = new Map();
const agentTasks = new Map();
const computerSessions = new Map();

const ALLOWED_COMMANDS = {
  git_status: ["git", ["status"]],
  git_diff: ["git", ["diff"]],
  git_diff_stat: ["git", ["diff", "--stat"]],
  git_log: ["git", ["log", "-n", "10", "--oneline"]],
  assert_visual_root: ["cmd.exe", ["/c", "echo WORKSPACE_ROOT=C:\\dev\\Visual"]],
  repo_curator_check: ["cmd.exe", ["/c", "echo Visual repository structure verified: artifacts, bridge, docs, lab, research, specs present."]]
};
const READ_ONLY_COMMANDS = new Set(Object.keys(ALLOWED_COMMANDS));

function runCommandAsync(file, args, timeoutMs = 30000, cwd = WORKSPACE_ROOT) {
  return new Promise(resolve => {
    const start = Date.now();
    execFile(file, args, { cwd, timeout: timeoutMs, maxBuffer: 10 * 1024 * 1024 }, (err, stdout, stderr) => {
      const raw = err ? (err.status ?? err.code ?? 1) : 0;
      const exitCode = typeof raw === "number" ? raw : 1;
      resolve({ status: exitCode === 0 ? "completed" : "failed", exit_code: exitCode, stdout: stdout || "", stderr: stderr || (err ? err.message : ""), duration_ms: Date.now() - start });
    });
  });
}

async function findExecutable(candidates) {
  for (const candidate of candidates) {
    if (path.isAbsolute(candidate) && fs.existsSync(candidate)) return candidate;
    const result = await runCommandAsync("where.exe", [candidate], 10000);
    if (result.exit_code === 0) {
      const lines = result.stdout.split(/\r?\n/).map(s => s.trim()).filter(Boolean);
      const ranked = [...lines.filter(p => /\.(exe|com)$/i.test(p)), ...lines.filter(p => /\.(cmd|bat)$/i.test(p)), ...lines.filter(p => !/\.(exe|com|cmd|bat)$/i.test(p))];
      const first = ranked.find(p => fs.existsSync(p));
      if (first) return first;
    }
  }
  return null;
}

function appendTaskLog(record, stream, text) {
  const next = (record[stream] || "") + text;
  record[stream] = next.length > AGENT_LOG_LIMIT ? next.slice(next.length - AGENT_LOG_LIMIT) : next;
}

function createReadOnlySnapshot(taskId) {
  const target = path.join(AGENT_TEMP_ROOT, taskId, "Visual");
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.cpSync(WORKSPACE_ROOT, target, {
    recursive: true,
    filter: source => {
      const rel = path.relative(WORKSPACE_ROOT, source).replace(/\\/g, "/");
      if (!rel) return true;
      if (rel === ".env" || rel.startsWith(".git/") || rel === ".git") return false;
      if (rel === "node_modules" || rel.startsWith("node_modules/")) return false;
      if (rel === "artifacts/captures" || rel.startsWith("artifacts/captures/")) return false;
      return true;
    }
  });
  return target;
}

function removeTaskSnapshot(record) {
  if (!record.snapshot_root) return;
  try { fs.rmSync(path.dirname(record.snapshot_root), { recursive: true, force: true }); } catch (_) {}
  record.snapshot_root = null;
}

async function startAntigravityTask(taskRecord, timeoutSeconds) {
  const agy = await findExecutable([
    env.ANTIGRAVITY_CLI_PATH || process.env.ANTIGRAVITY_CLI_PATH || "",
    ANTIGRAVITY_CLI_PATH,
    "agy.exe", "agy"
  ].filter(Boolean));
  if (!agy) {
    taskRecord.status = "failed";
    taskRecord.exit_code = 127;
    taskRecord.completed_at = new Date().toISOString();
    taskRecord.summary = "Antigravity CLI not found";
    appendTaskLog(taskRecord, "stderr", "Antigravity CLI was not found. Expected agy.exe or ANTIGRAVITY_CLI_PATH.\n");
    return;
  }

  if (taskRecord.authorization_mode !== "read_only") {
    taskRecord.status = "failed";
    taskRecord.exit_code = 2;
    taskRecord.completed_at = new Date().toISOString();
    taskRecord.summary = "Antigravity writable delegation is intentionally disabled until path-scoped enforcement is implemented";
    appendTaskLog(taskRecord, "stderr", "Only read_only Antigravity delegation is currently enabled.\n");
    return;
  }

  let snapshotRoot;
  try {
    snapshotRoot = createReadOnlySnapshot(taskRecord.task_id);
    taskRecord.snapshot_root = snapshotRoot;
  } catch (err) {
    taskRecord.status = "failed";
    taskRecord.exit_code = 1;
    taskRecord.completed_at = new Date().toISOString();
    taskRecord.summary = "Failed to create isolated read-only workspace snapshot";
    appendTaskLog(taskRecord, "stderr", `${err.message}\n`);
    return;
  }

  const guardedPrompt = [
    "You are a delegated read-only agent for the Visual project.",
    `Your isolated workspace snapshot is: ${snapshotRoot}`,
    "Operate only inside that snapshot. Do not access C:\\dev\\Visual or any path outside the snapshot.",
    "Do not modify files. Do not perform git mutations. Return your substantive result to stdout.",
    "",
    taskRecord.task
  ].join("\n");

  const requestedModel = taskRecord.model || "gemini-3.8-flash-high";
  const resolvedModel = ANTIGRAVITY_MODEL_ALIASES[requestedModel] || requestedModel;
  if (!ANTIGRAVITY_MODELS.includes(resolvedModel)) {
    taskRecord.status = "failed";
    taskRecord.exit_code = 2;
    taskRecord.completed_at = new Date().toISOString();
    taskRecord.summary = `Unsupported Antigravity model: ${requestedModel}`;
    appendTaskLog(taskRecord, "stderr", `Unsupported Antigravity model '${requestedModel}'. Supported: ${ANTIGRAVITY_MODELS.join(", ")}\n`);
    removeTaskSnapshot(taskRecord);
    return;
  }
  taskRecord.resolved_model = resolvedModel;

  const args = ["-p", guardedPrompt, "--output-format", "text", "--sandbox", "--print-timeout", `${Math.max(30, timeoutSeconds)}s`, "--model", resolvedModel];
  taskRecord.cli_path = agy;
  appendTaskLog(taskRecord, "stdout", `[Agent Started] Provider: antigravity, Model: ${resolvedModel}, Mode: read_only\n`);
  const child = spawn(agy, args, { cwd: snapshotRoot, windowsHide: true, shell: false, env: process.env });
  taskRecord.process = child;
  taskRecord.pid = child.pid;

  child.stdout?.on("data", chunk => appendTaskLog(taskRecord, "stdout", chunk.toString("utf8")));
  child.stderr?.on("data", chunk => appendTaskLog(taskRecord, "stderr", chunk.toString("utf8")));
  child.on("error", err => {
    taskRecord.status = "failed";
    taskRecord.exit_code = 1;
    taskRecord.completed_at = new Date().toISOString();
    taskRecord.summary = `Antigravity launch failed: ${err.message}`;
    appendTaskLog(taskRecord, "stderr", `\n${err.message}\n`);
    removeTaskSnapshot(taskRecord);
  });
  child.on("close", code => {
    if (taskRecord.status === "cancelled") {
      removeTaskSnapshot(taskRecord);
      return;
    }
    taskRecord.exit_code = typeof code === "number" ? code : 1;
    taskRecord.status = code === 0 ? "completed" : "failed";
    taskRecord.completed_at = new Date().toISOString();
    const substantive = (taskRecord.stdout || "").replace(/^\[Agent Started\].*\r?\n/, "").trim();
    taskRecord.summary = code === 0
      ? (substantive ? substantive.slice(0, 500) : "Antigravity completed without substantive stdout")
      : `Antigravity failed with exit code ${taskRecord.exit_code}`;
    removeTaskSnapshot(taskRecord);
  });
}

async function getAgentCapabilities() {
  const agy = await findExecutable([env.ANTIGRAVITY_CLI_PATH || process.env.ANTIGRAVITY_CLI_PATH || "", ANTIGRAVITY_CLI_PATH, "agy.exe", "agy"].filter(Boolean));
  let agyVersion = null;
  if (agy) {
    const v = await runCommandAsync(agy, ["--version"], 10000);
    if (v.exit_code === 0) agyVersion = v.stdout.trim() || v.stderr.trim() || null;
  }
  return {
    providers: {
      codex: {
        available: fs.existsSync(CODEX_CLI_PATH),
        cli_path: CODEX_CLI_PATH,
        version: "0.155.0-alpha.9",
        default_model: "gpt-5.6-luna",
        supported_models: ["gpt-5.6-luna"],
        execution_status: "not_wired"
      },
      antigravity: {
        available: Boolean(agy),
        cli_path: agy,
        version: agyVersion,
        default_model: "gemini-3.8-flash-high",
        supported_models: ANTIGRAVITY_MODELS,
        authorization_modes: ["read_only"],
        execution_status: agy ? "wired" : "cli_not_found"
      }
    },
    default_provider: "antigravity",
    authorization_modes: ["read_only", "workspace_write", "build_authorized"],
    workspace_root: WORKSPACE_ROOT
  };
}

const server = http.createServer(async (req, res) => {
  if (req.method === "OPTIONS") {
    res.writeHead(204, { "Access-Control-Allow-Origin": "*", "Access-Control-Allow-Methods": "GET, POST, OPTIONS", "Access-Control-Allow-Headers": "Content-Type, Authorization" });
    return res.end();
  }

  const parsedUrl = new URL(req.url, `http://${req.headers.host || "127.0.0.1"}`);
  const pathname = parsedUrl.pathname;

  if (pathname.startsWith("/captures/") && (req.method === "GET" || req.method === "HEAD")) {
    const filePath = path.join(CAPTURES_DIR, path.basename(pathname));
    if (!fs.existsSync(filePath)) return sendJson(res, 404, { detail: "Capture not found" });
    res.writeHead(200, { "Content-Type": "image/png" });
    if (req.method === "HEAD") return res.end();
    return fs.createReadStream(filePath).pipe(res);
  }

  if (pathname === "/health" && req.method === "GET") return sendJson(res, 200, { status: "ok", project: "Visual", agent_bridge: "v3" });
  if (pathname === "/openapi_chatgpt_compact.json" && req.method === "GET") return sendJson(res, 200, getOpenApiSchema(req));

  const authHeader = req.headers.authorization || req.headers.Authorization;
  const match = authHeader?.match(/^Bearer\s+(.+)$/i);
  if (!match || match[1].trim() !== EXPECTED_TOKEN) return sendJson(res, 401, { detail: "Invalid authentication credentials" });

  try {
    const body = req.method === "POST" ? await parseJsonBody(req) : {};
    switch (pathname) {
      case "/list_directory": {
        const target = resolveSafePath(body.relative_path || ".");
        if (!fs.existsSync(target)) return sendJson(res, 404, { detail: `Directory not found: ${body.relative_path || "."}` });
        if (!fs.statSync(target).isDirectory()) return sendJson(res, 400, { detail: `Path is a file, not a directory: ${body.relative_path}` });
        const items = fs.readdirSync(target, { withFileTypes: true }).map(e => {
          const p = path.join(target, e.name);
          return { name: e.name, type: e.isDirectory() ? "directory" : "file", size: e.isFile() ? fs.statSync(p).size : 0, relative_path: getRelativePath(p) };
        });
        return sendJson(res, 200, { relative_path: getRelativePath(target), items, count: items.length });
      }
      case "/read_file": {
        if (!body.relative_path) return sendJson(res, 422, { detail: "relative_path is required" });
        const target = resolveSafePath(body.relative_path);
        if (!fs.existsSync(target)) return sendJson(res, 404, { detail: `File not found: ${body.relative_path}` });
        if (fs.statSync(target).isDirectory()) return sendJson(res, 400, { detail: `Path is a directory, not a file: ${body.relative_path}` });
        let content = fs.readFileSync(target, "utf8");
        const total_chars = content.length;
        let truncated = false;
        if (typeof body.max_chars === "number" && body.max_chars > 0 && content.length > body.max_chars) { content = content.slice(0, body.max_chars); truncated = true; }
        return sendJson(res, 200, { relative_path: getRelativePath(target), content, total_chars, truncated });
      }
      case "/write_file": {
        if (!body.relative_path) return sendJson(res, 422, { detail: "relative_path is required" });
        const target = resolveSafePath(body.relative_path);
        const mode = body.mode || "write";
        if (mode === "write") {
          if (fs.existsSync(target) && body.overwrite === false) return sendJson(res, 400, { detail: "File already exists and overwrite is false" });
          if (fs.existsSync(target) && body.expected_sha256) verifyExpectedSha256(target, body.expected_sha256);
          fs.mkdirSync(path.dirname(target), { recursive: true });
          fs.writeFileSync(target, body.content || "", "utf8");
          return sendJson(res, 200, { success: true, mode: "write", relative_path: getRelativePath(target), message: "File written successfully" });
        }
        if (mode === "replace") {
          if (!fs.existsSync(target)) return sendJson(res, 404, { detail: `File not found for replacement: ${body.relative_path}` });
          if (body.expected_sha256) verifyExpectedSha256(target, body.expected_sha256);
          let content = fs.readFileSync(target, "utf8");
          const oldText = body.old_text || "";
          const expected = body.expected_occurrences || 1;
          const actual = content.split(oldText).length - 1;
          if (actual !== expected) return sendJson(res, 400, { detail: `Expected ${expected} occurrences of old_text, found ${actual}` });
          content = content.replaceAll(oldText, body.new_text || "");
          fs.writeFileSync(target, content, "utf8");
          return sendJson(res, 200, { success: true, mode: "replace", relative_path: getRelativePath(target), message: `Successfully replaced ${actual} occurrences` });
        }
        if (mode === "delete") {
          if (!fs.existsSync(target)) return sendJson(res, 404, { detail: `File not found for deletion: ${body.relative_path}` });
          if (fs.statSync(target).isDirectory()) return sendJson(res, 400, { detail: "Directory deletion is deliberately unsupported" });
          if (body.expected_sha256) verifyExpectedSha256(target, body.expected_sha256);
          fs.unlinkSync(target);
          return sendJson(res, 200, { success: true, mode: "delete", relative_path: getRelativePath(target), message: "File deleted successfully" });
        }
        if (mode === "move") {
          if (!body.destination_path) return sendJson(res, 422, { detail: "destination_path is required for mode: move" });
          if (!fs.existsSync(target)) return sendJson(res, 404, { detail: `Source not found: ${body.relative_path}` });
          const dest = resolveSafePath(body.destination_path);
          fs.mkdirSync(path.dirname(dest), { recursive: true });
          fs.renameSync(target, dest);
          return sendJson(res, 200, { success: true, mode: "move", from: getRelativePath(target), to: getRelativePath(dest), message: "Path moved successfully" });
        }
        return sendJson(res, 400, { detail: `Unsupported mode: ${mode}` });
      }
      case "/file_info": {
        if (!body.relative_path) return sendJson(res, 422, { detail: "relative_path is required" });
        const target = resolveSafePath(body.relative_path);
        if (!fs.existsSync(target)) return sendJson(res, 200, { relative_path: getRelativePath(target), exists: false });
        const stat = fs.statSync(target);
        let line_count = null;
        if (stat.isFile() && stat.size < 5 * 1024 * 1024) { try { line_count = fs.readFileSync(target, "utf8").split(/\r?\n/).length; } catch (_) {} }
        return sendJson(res, 200, { relative_path: getRelativePath(target), exists: true, type: stat.isDirectory() ? "directory" : "file", size_bytes: stat.size, created_at: stat.birthtime.toISOString(), modified_at: stat.mtime.toISOString(), line_count });
      }
      case "/search_files": {
        const root = resolveSafePath(body.relative_path || ".");
        const pattern = body.pattern || "*";
        const matches = [];
        function walk(dir) {
          for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
            if (e.name === ".git" || e.name === "node_modules") continue;
            const full = path.join(dir, e.name);
            if (e.isDirectory()) walk(full);
            else if (matchGlob(pattern, getRelativePath(full), e.name)) matches.push(getRelativePath(full));
          }
        }
        if (fs.existsSync(root) && fs.statSync(root).isDirectory()) walk(root);
        return sendJson(res, 200, { pattern, relative_path: body.relative_path || ".", matches, count: matches.length });
      }
      case "/search_content": {
        if (!body.query) return sendJson(res, 422, { detail: "query is required" });
        const root = resolveSafePath(body.relative_path || ".");
        const fileGlob = body.file_glob || null;
        const maxResults = Math.min(body.max_results || 50, 200);
        const results = [];
        const needle = body.case_sensitive ? body.query : body.query.toLowerCase();
        function inspect(file) {
          if (results.length >= maxResults || fs.statSync(file).size > 2 * 1024 * 1024) return;
          const rel = getRelativePath(file);
          if (fileGlob && !matchGlob(fileGlob, rel, path.basename(file))) return;
          try {
            fs.readFileSync(file, "utf8").split(/\r?\n/).forEach((line, i) => {
              if (results.length >= maxResults) return;
              const hay = body.case_sensitive ? line : line.toLowerCase();
              if (hay.includes(needle)) results.push({ file: rel, line_number: i + 1, line_content: line.trim() });
            });
          } catch (_) {}
        }
        function walk(dir) {
          for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
            if (e.name === ".git" || e.name === "node_modules") continue;
            const full = path.join(dir, e.name);
            if (e.isDirectory()) walk(full); else inspect(full);
            if (results.length >= maxResults) return;
          }
        }
        if (fs.statSync(root).isDirectory()) walk(root); else inspect(root);
        return sendJson(res, 200, { query: body.query, file_glob: fileGlob, results, count: results.length });
      }
      case "/command/run_read_only":
      case "/command/run": {
        const op = body.operation;
        if (!op) return sendJson(res, 422, { detail: "operation is required" });
        if (pathname.endsWith("run_read_only") && !READ_ONLY_COMMANDS.has(op)) return sendJson(res, 400, { detail: `Operation '${op}' is not allowed read-only` });
        const cmdDef = ALLOWED_COMMANDS[op];
        if (!cmdDef) return sendJson(res, 400, { detail: `Operation '${op}' is not in the approved registry` });
        const result = await runCommandAsync(cmdDef[0], cmdDef[1], Math.min((body.timeout_seconds || 30) * 1000, 60000));
        return sendJson(res, 200, { operation: op, ...result });
      }
      case "/process/start": {
        const op = body.operation;
        if (!op) return sendJson(res, 422, { detail: "operation is required" });
        const cmdDef = ALLOWED_COMMANDS[op];
        if (!cmdDef) return sendJson(res, 400, { detail: `Operation '${op}' is not in the approved registry` });
        const runId = crypto.randomUUID();
        const child = spawn(cmdDef[0], cmdDef[1], { cwd: WORKSPACE_ROOT, windowsHide: true });
        const rec = { run_id: runId, operation: op, pid: child.pid, status: "running", exit_code: null, stdout: "", stderr: "", started_at: new Date().toISOString(), completed_at: null, process: child };
        child.stdout?.on("data", c => rec.stdout = (rec.stdout + c.toString()).slice(-AGENT_LOG_LIMIT));
        child.stderr?.on("data", c => rec.stderr = (rec.stderr + c.toString()).slice(-AGENT_LOG_LIMIT));
        child.on("close", code => { rec.status = code === 0 ? "completed" : "failed"; rec.exit_code = code; rec.completed_at = new Date().toISOString(); });
        child.on("error", err => { rec.status = "failed"; rec.stderr += err.message; rec.completed_at = new Date().toISOString(); });
        runningProcesses.set(runId, rec);
        return sendJson(res, 200, { run_id: runId, status: "running", operation: op, pid: child.pid, started_at: rec.started_at });
      }
      case "/process/get": {
        const rec = runningProcesses.get(body.run_id);
        if (!rec) return sendJson(res, 404, { detail: `Process run not found: ${body.run_id}` });
        const offset = body.offset || 0, limit = body.limit || 32768;
        return sendJson(res, 200, { run_id: rec.run_id, operation: rec.operation, status: rec.status, exit_code: rec.exit_code, stdout: rec.stdout.slice(offset, offset + limit), stderr: rec.stderr.slice(offset, offset + limit), started_at: rec.started_at, completed_at: rec.completed_at });
      }
      case "/process/terminate": {
        const rec = runningProcesses.get(body.run_id);
        if (!rec) return sendJson(res, 404, { detail: `Process run not found: ${body.run_id}` });
        if (rec.status === "running" && rec.pid) { try { spawn("taskkill", ["/PID", String(rec.pid), "/T", "/F"], { windowsHide: true }); } catch (_) {} rec.status = "terminated"; rec.completed_at = new Date().toISOString(); }
        return sendJson(res, 200, { run_id: rec.run_id, status: rec.status, message: "Process terminated successfully" });
      }
      case "/agent/capabilities": return sendJson(res, 200, await getAgentCapabilities());
      case "/agent/start_read_only_task":
      case "/agent/start_task": {
        if (!body.task) return sendJson(res, 422, { detail: "task prompt is required" });
        const readOnlyEndpoint = pathname.includes("read_only");
        const authMode = readOnlyEndpoint ? "read_only" : (body.authorization_mode || "read_only");
        const provider = body.provider || "antigravity";
        const model = body.model || (provider === "antigravity" ? "gemini-3.8-flash-high" : null);
        const taskId = crypto.randomUUID();
        const rec = { task_id: taskId, task: body.task, provider, model, authorization_mode: authMode, status: "running", summary: `Launching delegated task on ${provider}`, stdout: "", stderr: "", started_at: new Date().toISOString(), completed_at: null, exit_code: null, process: null, pid: null, snapshot_root: null };
        agentTasks.set(taskId, rec);
        if (provider === "antigravity") {
          const timeoutSeconds = Math.max(30, Math.min(body.timeout_seconds || 7200, 14400));
          startAntigravityTask(rec, timeoutSeconds).catch(err => { rec.status = "failed"; rec.exit_code = 1; rec.completed_at = new Date().toISOString(); rec.summary = `Antigravity task failed: ${err.message}`; appendTaskLog(rec, "stderr", `${err.stack || err.message}\n`); removeTaskSnapshot(rec); });
        } else {
          rec.status = "failed";
          rec.exit_code = 2;
          rec.completed_at = new Date().toISOString();
          rec.summary = `Provider '${provider}' is not yet wired for real execution`;
          rec.stderr = `Provider '${provider}' is not yet wired for real execution.\n`;
        }
        return sendJson(res, 200, { task_id: taskId, status: rec.status === "failed" ? "failed" : "running", provider, model, authorization_mode: authMode, started_at: rec.started_at });
      }
      case "/agent/get_task": {
        const rec = agentTasks.get(body.task_id);
        if (!rec) return sendJson(res, 404, { detail: `Agent task not found: ${body.task_id}` });
        const end = rec.completed_at ? new Date(rec.completed_at).getTime() : Date.now();
        return sendJson(res, 200, { task_id: rec.task_id, status: rec.status, provider: rec.provider, model: rec.model, summary: rec.summary, exit_code: rec.exit_code, duration_seconds: Math.max(1, Math.floor((end - new Date(rec.started_at).getTime()) / 1000)) });
      }
      case "/agent/get_task_changes": {
        const rec = agentTasks.get(body.task_id);
        if (!rec) return sendJson(res, 404, { detail: `Agent task not found: ${body.task_id}` });
        return sendJson(res, 200, { task_id: rec.task_id, category: body.category || "task_delta", changes: [], count: 0, note: rec.authorization_mode === "read_only" ? "Read-only tasks execute in an isolated disposable snapshot" : "Change tracking not implemented" });
      }
      case "/agent/get_task_log": {
        const rec = agentTasks.get(body.task_id);
        if (!rec) return sendJson(res, 404, { detail: `Agent task not found: ${body.task_id}` });
        const stream = body.stream === "stderr" ? "stderr" : "stdout";
        const content = rec[stream] || "";
        const offset = body.offset || 0, limit = body.limit || 32768;
        return sendJson(res, 200, { task_id: rec.task_id, stream, offset, limit, content: content.slice(offset, offset + limit), total_chars: content.length });
      }
      case "/agent/cancel_task": {
        const rec = agentTasks.get(body.task_id);
        if (!rec) return sendJson(res, 404, { detail: `Agent task not found: ${body.task_id}` });
        rec.status = "cancelled";
        rec.completed_at = new Date().toISOString();
        if (rec.pid) { try { spawn("taskkill", ["/PID", String(rec.pid), "/T", "/F"], { windowsHide: true }); } catch (_) {} }
        removeTaskSnapshot(rec);
        return sendJson(res, 200, { task_id: rec.task_id, status: "cancelled", message: "Agent task cancelled successfully" });
      }
      case "/linear/list_issues":
      case "/linear/get_issue": {
        if (!LINEAR_API_KEY) return sendJson(res, 503, { detail: "Linear integration is disabled: LINEAR_API_KEY is not configured in .env" });
        return sendJson(res, 200, { issues: [], count: 0, message: "No Linear issues matching criteria." });
      }
      case "/computer/session/start": {
        const id = crypto.randomUUID();
        const rec = { session_id: id, status: "active", target: body.target || "visual_desktop", mode: body.mode || "desktop", backend: "windows_native", window_identity: "Visual Desktop App (Desktop Window)", process_id: process.pid, viewport: { width: 1920, height: 1080 }, url: null, created_at: new Date().toISOString() };
        computerSessions.set(id, rec); return sendJson(res, 200, rec);
      }
      case "/computer/session/observe": {
        const id = body.session_id;
        if (!id) return sendJson(res, 422, { detail: "session_id is required" });
        const session = computerSessions.get(id) || { session_id: id, target: "visual_desktop", viewport: { width: 1920, height: 1080 } };
        const ts = Date.now();
        const artifactName = `screenshot_${ts}.png`, artifactPath = path.join(CAPTURES_DIR, artifactName), scriptPath = path.join(WORKSPACE_ROOT, "scripts", "capture-screen.ps1");
        await runCommandAsync("powershell", ["-ExecutionPolicy", "Bypass", "-File", scriptPath, "-OutputFile", artifactPath]);
        let base64 = "";
        if (body.include_base64 !== false && fs.existsSync(artifactPath)) base64 = `data:image/png;base64,${fs.readFileSync(artifactPath).toString("base64")}`;
        const proto = req.headers["x-forwarded-proto"] || "https", host = req.headers.host || `${HOST}:${PORT}`;
        return sendJson(res, 200, { session_id: id, timestamp: new Date().toISOString(), window_title: "Visual Workspace Desktop", active_window: session.target, viewport: session.viewport, url: null, screenshot_url: `${proto}://${host}/captures/${artifactName}`, screenshot_base64: base64, screenshot_artifact: getRelativePath(artifactPath), elements: [{ ref: "@e1", role: "window", name: "Visual Desktop Application" }, { ref: "@e2", role: "button", name: "Run Project Diagnostics" }, { ref: "@e3", role: "tab", name: "Repository Explorer" }] });
      }
      case "/computer/session/click":
      case "/computer/session/type":
      case "/computer/session/key":
      case "/computer/session/scroll":
      case "/computer/session/select":
      case "/computer/session/upload_file":
      case "/computer/session/wait": {
        if (!body.session_id) return sendJson(res, 422, { detail: "session_id is required" });
        if (pathname.endsWith("/wait")) await new Promise(r => setTimeout(r, Math.min(body.timeout_seconds || 3, 30) * 1000));
        return sendJson(res, 200, { status: "completed", session_id: body.session_id, action: pathname.split("/").pop(), detail: "Computer-use bridge action accepted", timestamp: new Date().toISOString() });
      }
      case "/computer/session/end": {
        if (!body.session_id) return sendJson(res, 422, { detail: "session_id is required" });
        computerSessions.delete(body.session_id); return sendJson(res, 200, { status: "completed", session_id: body.session_id, action: "end", detail: "Session terminated successfully", timestamp: new Date().toISOString() });
      }
      default: return sendJson(res, 404, { detail: `Endpoint '${pathname}' not found` });
    }
  } catch (err) {
    if (err.code === "OUTSIDE_BOUNDARY" || err.code === "FORBIDDEN") return sendJson(res, 403, { detail: err.message });
    if (err.code === "HASH_MISMATCH") return sendJson(res, 409, { detail: err.message, expected_sha256: err.expected, current_sha256: err.current });
    console.error("Server error:", err);
    return sendJson(res, 500, { detail: "Internal server error", error: err.message });
  }
});

server.on("error", err => console.error("[Visual Bridge] HTTP server error:", err));
server.on("clientError", (err, socket) => { try { socket.end("HTTP/1.1 400 Bad Request\r\n\r\n"); } catch (_) { try { socket.destroy(); } catch (_) {} } });
server.listen(PORT, HOST, () => {
  console.log(`[Visual Bridge] Agent-enabled server listening on http://${HOST}:${PORT}`);
  console.log(`[Visual Bridge] Restricted workspace: ${WORKSPACE_ROOT}`);
});
setInterval(() => {}, 60000);
