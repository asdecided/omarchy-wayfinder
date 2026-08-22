.pragma library

var DEFAULT_ENDPOINT = "http://127.0.0.1:8088"

function normalizedEndpoint(value) {
  var endpoint = String(value || "").trim()
  if (endpoint === "") endpoint = DEFAULT_ENDPOINT
  if (!/^https?:\/\//i.test(endpoint)) endpoint = "http://" + endpoint
  return endpoint.replace(/\/+$/, "")
}

function boundedInteger(value, fallback, minimum, maximum) {
  var parsed = parseInt(String(value), 10)
  if (!isFinite(parsed)) parsed = fallback
  return Math.max(minimum, Math.min(maximum, parsed))
}

function parsedObject(raw) {
  try {
    var value = JSON.parse(String(raw || ""))
    return value && typeof value === "object" && !Array.isArray(value) ? value : null
  } catch (_) {
    return null
  }
}

function doctor(raw) {
  var value = parsedObject(raw)
  if (!value || String(value.schema_version || "") !== "1"
      || typeof value.ok !== "boolean" || !isFinite(Number(value.destinations))) {
    return {
      valid: false,
      ok: false,
      config: "",
      destinations: 0,
      missingEnvironment: [],
      gatewayReachable: false
    }
  }
  return {
    valid: true,
    ok: value.ok === true,
    config: String(value.config || ""),
    destinations: Math.max(0, Number(value.destinations || 0)),
    missingEnvironment: Array.isArray(value.missing_environment)
      ? value.missing_environment.map(String) : [],
    gatewayReachable: value.gateway_reachable === true
  }
}

function setupState(state) {
  var input = state || {}
  var missing = Array.isArray(input.missingEnvironment)
    ? input.missingEnvironment.map(String) : []
  var missingDetail = missing.length > 0
    ? " Missing environment: " + missing.join(", ") + "."
    : ""

  if (!input.localEndpoint) {
    return { ready: false, actionable: false, step: "remote", status: "",
      title: "Remote endpoint", action: "Remote endpoint",
      detail: "This endpoint is observed but not managed." }
  }
  if (!input.binaryInstalled) {
    return { ready: false, actionable: false, step: "binary", status: "Router not installed",
      title: "Install the Router", action: "Router missing",
      detail: "Run the plugin installer to add the checksum-verified native Router." }
  }
  if (String(input.configPath || "") === "") {
    return { ready: false, actionable: false, step: "path", status: "Preparing setup",
      title: "Resolve the policy path", action: "Preparing…",
      detail: "Wayfinder is resolving a user-owned configuration path." }
  }
  if (!input.configChecked) {
    return { ready: false, actionable: false, step: "probe", status: "Checking setup",
      title: "Check existing setup", action: "Checking…",
      detail: "Wayfinder is checking for an existing policy without changing it." }
  }
  if (!input.configExists) {
    return { ready: false, actionable: true, step: "policy", status: "Setup required",
      title: "Create a local policy", action: "Set up Wayfinder",
      detail: "Creates a no-clobber local policy, validates it, and installs the user service." }
  }
  if (!input.doctorChecked) {
    return { ready: false, actionable: true, step: "doctor", status: "Checking policy",
      title: "Validate the policy", action: "Check policy",
      detail: "Runs the native doctor against the existing policy before service installation." }
  }
  if (!input.configValid) {
    return { ready: false, actionable: true, step: "repair", status: "Policy needs attention",
      title: "Repair the existing policy", action: "Recheck policy",
      detail: "The existing policy was not changed. Fix it, then run the check again." }
  }
  if (!input.unitInstalled) {
    return { ready: false, actionable: true, step: "service", status: "Service not installed",
      title: "Install the user service", action: "Install service",
      detail: "The policy is valid." + missingDetail
        + " Installation uses the reviewed systemd user-service boundary." }
  }
  if (!input.systemdActive) {
    return { ready: true, actionable: true, step: "start", status: "",
      title: "Start the Router", action: "Start service",
      detail: "The service is installed but stopped." + missingDetail }
  }
  if (!input.reachable) {
    return { ready: true, actionable: true, step: "restart", status: "",
      title: "Recover the Router", action: "Restart service",
      detail: "The service is active but its loopback gateway is not reachable." + missingDetail }
  }
  return { ready: true, actionable: true, step: "ready", status: "",
    title: "Wayfinder is ready", action: "Restart service",
    detail: missingDetail.trim() }
}

function health(raw) {
  var value = parsedObject(raw)
  if (!value || (value.status !== "ok" && value.status !== "degraded")) {
    return { valid: false, status: "unreachable", models: [], missingKeys: [], offline: false }
  }
  return {
    valid: true,
    status: String(value.status),
    models: Array.isArray(value.models) ? value.models.map(String) : [],
    missingKeys: Array.isArray(value.missing_keys) ? value.missing_keys.map(String) : [],
    offline: value.offline === true
  }
}

function models(raw) {
  var value = parsedObject(raw)
  if (!value || !Array.isArray(value.models)) return { valid: false, models: [], dryRun: false }
  var output = []
  for (var i = 0; i < value.models.length; i++) {
    var model = value.models[i]
    if (!model || typeof model !== "object" || !model.name) continue
    output.push({
      name: String(model.name),
      endpoint: String(model.endpoint || ""),
      provider: String(model.provider || ""),
      providerModel: String(model.model || ""),
      tier: model.tier === null || model.tier === undefined ? "" : String(model.tier),
      keyReady: model.key_ok === true
    })
  }
  return { valid: true, models: output, dryRun: value.dry_run === true }
}

function recent(raw) {
  var value = parsedObject(raw)
  if (!value || !Array.isArray(value.recent)) return { valid: false, total: 0, byModel: {}, recent: [] }
  var entries = []
  for (var i = 0; i < value.recent.length; i++) {
    var item = value.recent[i]
    if (!item || typeof item !== "object") continue
    entries.push({
      requestId: String(item.request_id || ""),
      model: String(item.model || "unknown"),
      score: Number(item.score || 0),
      mode: String(item.mode || "scored"),
      timestamp: Number(item.ts || 0),
      saved: item.cost && isFinite(Number(item.cost.saved)) ? Number(item.cost.saved) : null,
      unit: item.cost ? String(item.cost.unit || "") : "",
      estimated: !!(item.cost && item.cost.estimated),
      cache: String(item.cache || "")
    })
  }
  return {
    valid: true,
    total: Math.max(0, Number(value.total || 0)),
    byModel: value.by_model && typeof value.by_model === "object" ? value.by_model : {},
    recent: entries
  }
}

function savings(raw) {
  var value = parsedObject(raw)
  if (!value) {
    return { valid: false, requests: 0, saved: 0, savedPct: 0, unit: "relative", priced: false }
  }
  return {
    valid: true,
    requests: Math.max(0, Number(value.requests || 0)),
    saved: Number(value.saved || 0),
    savedPct: Number(value.saved_pct || 0),
    unit: String(value.unit || "relative"),
    priced: value.priced === true,
    realized: Number(value.realized || 0),
    baseline: Number(value.baseline || 0)
  }
}

function isLoopbackEndpoint(endpoint) {
  return /^https?:\/\/(127(?:\.\d+){3}|localhost|\[::1\])(?::\d+)?(?:\/|$)/i.test(String(endpoint || ""))
}

function isLocalModel(model) {
  if (!model) return false
  var tier = String(model.tier || "").toLowerCase()
  var provider = String(model.provider || "").toLowerCase()
  return tier === "local" || tier === "cheap" || tier === "low"
    || provider === "ollama" || provider === "lm-studio" || provider === "llama.cpp"
    || provider === "vllm" || isLoopbackEndpoint(model.endpoint)
}

function routingStats(report, modelList) {
  var modelMap = {}
  for (var i = 0; i < modelList.length; i++) modelMap[modelList[i].name] = modelList[i]
  var local = 0
  var hosted = 0
  var unknown = 0
  var counts = report && report.byModel ? report.byModel : {}
  for (var name in counts) {
    var count = Math.max(0, Number(counts[name] || 0))
    if (!modelMap[name]) unknown += count
    else if (isLocalModel(modelMap[name])) local += count
    else hosted += count
  }
  var total = local + hosted + unknown
  return {
    local: local,
    hosted: hosted,
    unknown: unknown,
    total: total,
    localFraction: total > 0 ? local / total : 0
  }
}

function serviceInstallArguments(endpoint, configPath) {
  var normalized = normalizedEndpoint(endpoint)
  var match = normalized.match(/^http:\/\/(127(?:\.\d+){3}|localhost|\[::1\])(?::(\d+))?$/i)
  if (!match) return null
  var port = match[2] || "8088"
  var numericPort = Number(port)
  if (!isFinite(numericPort) || numericPort < 1 || numericPort > 65535) return null
  var host = match[1] === "[::1]" ? "::1" : match[1]
  var args = ["service", "install", "--host", host, "--port", String(numericPort)]
  var config = String(configPath || "").trim()
  if (config !== "") args.push("--config", config)
  return args
}

function shortModel(name) {
  var value = String(name || "unknown")
  return value.length > 28 ? value.substring(0, 25) + "…" : value
}

function fixed(value, digits) {
  var number = Number(value)
  return isFinite(number) ? number.toFixed(digits) : "0"
}

function savingsLabel(report) {
  if (!report || !report.valid || report.requests === 0) return "No accounted requests"
  if (report.priced && report.unit === "usd") return "$" + fixed(report.saved, 2) + " saved"
  return fixed(report.savedPct, 1) + "% saved"
}

function relativeTime(timestamp, nowMs) {
  var delta = Math.max(0, Number(nowMs || Date.now()) - Number(timestamp || 0) * 1000)
  if (delta < 60000) return "now"
  var minutes = Math.floor(delta / 60000)
  if (minutes < 60) return minutes + "m"
  var hours = Math.floor(minutes / 60)
  if (hours < 24) return hours + "h"
  return Math.floor(hours / 24) + "d"
}
