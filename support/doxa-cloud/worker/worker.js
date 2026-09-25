const ALLOWED_ACTIONS = new Set([
  "run_minimal_capture_probe",
  "recheck_doxa_devices",
  "retry_post_install_health",
  "collect_support_bundle",
  "escalate_it",
]);

function json(value, status = 200) {
  return new Response(JSON.stringify(value), {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "cache-control": "no-store",
    },
  });
}

function deterministicDecision(state) {
  const expected = Math.max(1, Number(state.expected_display_count || 2));
  const active = Number(state.active_displays || 0);
  if (state.windows_supported === false) {
    return { outcome: "unsupported_os", action: "stop", reason: "This Windows version is outside the supported baseline.", used_ai: false };
  }
  if (state.doxa_hardware && state.doxa_hardware !== "expected") {
    return { outcome: "wrong_hardware", action: "stop", reason: "The expected Doxa hardware profile was not detected.", used_ai: false };
  }
  if (state.enterprise_managed === true && state.deployment_context !== "system") {
    return { outcome: "needs_it", action: "escalate_it", reason: "Managed installation needs the enterprise deployment context.", used_ai: false, needs_it: true };
  }
  if (state.app_control === "blocked") {
    return { outcome: "needs_it", action: "escalate_it", reason: "Application Control blocked the approved package.", used_ai: false, needs_it: true };
  }
  if (state.pending_reboot === true) {
    return { outcome: "reboot_required", action: "reboot", reason: "Windows servicing reports a pending reboot.", used_ai: false };
  }
  if (active < expected) {
    return { outcome: "waiting_for_display", action: "wait_for_display", reason: `Expected ${expected} active displays but Windows reports ${active}.`, used_ai: false };
  }
  if (state.driver_state === "missing") {
    return { outcome: "driver_missing", action: "install_approved_driver", reason: "The approved display driver is missing.", used_ai: false };
  }
  if (state.visual_installed === false) {
    return { outcome: "visual_missing", action: "install_visual", reason: "Visual is not installed.", used_ai: false };
  }
  if (state.post_install_health === "pass") {
    return { outcome: "installed_ok", action: "finish", reason: "The post-install graphics health check passed.", used_ai: false };
  }
  if (state.network_to_doxa_cloud && state.network_to_doxa_cloud !== "reachable") {
    return { outcome: "needs_support_offline", action: "collect_support_bundle", reason: "Cloud planning is unavailable.", used_ai: false };
  }
  return null;
}

function normalizePlan(text) {
  let value = String(text || "").trim();
  value = value.replace(/^```(?:json)?\s*/i, "").replace(/\s*```$/, "");
  try { return JSON.parse(value); } catch {}
  const first = value.indexOf("{");
  const last = value.lastIndexOf("}");
  if (first >= 0 && last > first) return JSON.parse(value.slice(first, last + 1));
  throw new Error("Planner returned no valid JSON object.");
}

function plannerPrompt(state) {
  const allowed = JSON.stringify([...ALLOWED_ACTIONS]);
  return `You are the exception planner for the controlled Doxa Windows installer.
The normal installer is deterministic. You are called only when deterministic checks cannot explain the remaining failure.
Choose exactly one action from the allowed list. Never invent commands, scripts, registry changes, driver replacements, security bypasses, downloads, or additional actions.
If enterprise policy is likely involved, choose escalate_it. If evidence is insufficient, prefer a diagnostic action or collect_support_bundle.

Machine state:
${JSON.stringify(state)}

Allowed actions:
${allowed}

Return JSON only with exactly these fields:
{"decision":"diagnose|escalate","action":"one allowed action id","reason":"short plain-English reason","confidence":"low|medium|high","needs_it":true|false}`;
}

async function callDify(env, state, clientId) {
  if (!env.DIFY_API_KEY) throw new Error("DIFY_API_KEY is not configured.");
  const response = await fetch("https://api.dify.ai/v1/chat-messages", {
    method: "POST",
    headers: {
      authorization: `Bearer ${env.DIFY_API_KEY}`,
      "content-type": "application/json",
    },
    body: JSON.stringify({
      inputs: {},
      query: plannerPrompt(state),
      response_mode: "blocking",
      conversation_id: "",
      user: `doxa-${String(clientId || "installer").replace(/[^A-Za-z0-9_-]/g, "").slice(0, 48) || "installer"}`,
    }),
  });
  if (!response.ok) throw new Error(`Dify returned HTTP ${response.status}.`);
  const payload = await response.json();
  if (typeof payload.answer !== "string") throw new Error("Dify response did not contain answer.");
  return normalizePlan(payload.answer);
}

export default {
  async fetch(request, env) {
    try {
      const url = new URL(request.url);
      if (request.method === "GET" && url.pathname === "/health") {
        return json({ ok: true, service: "doxa-installer-planner", provider: "dify", schema_version: "1" });
      }
      if (request.method !== "POST" || url.pathname !== "/v1/plan") return json({ error: "not_found" }, 404);
      if (env.DOXA_CLIENT_TOKEN) {
        const auth = request.headers.get("authorization") || "";
        if (auth !== `Bearer ${env.DOXA_CLIENT_TOKEN}`) return json({ error: "unauthorized" }, 401);
      }
      const length = Number(request.headers.get("content-length") || 0);
      if (length > 65536) return json({ error: "request_too_large" }, 413);
      const input = await request.json();
      if (!input || typeof input !== "object" || !input.state || typeof input.state !== "object") {
        return json({ error: "invalid_request" }, 400);
      }

      const deterministic = deterministicDecision(input.state);
      if (deterministic) {
        return json({ schema_version: "1", decision: { ...deterministic, provider: "deterministic" } });
      }

      const plan = await callDify(env, input.state, input.client_id);
      const action = String(plan.action || "");
      if (!ALLOWED_ACTIONS.has(action)) throw new Error(`Planner selected action outside allowlist: ${action}`);
      return json({
        schema_version: "1",
        decision: {
          outcome: "ai_plan",
          action,
          reason: String(plan.reason || ""),
          used_ai: true,
          confidence: String(plan.confidence || "low"),
          needs_it: Boolean(plan.needs_it),
          provider: "dify",
        },
      });
    } catch (error) {
      return json({ error: "planner_failed", message: String(error?.message || error) }, 502);
    }
  },
};
