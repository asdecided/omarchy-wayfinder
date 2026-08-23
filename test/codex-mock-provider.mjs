import { createServer } from "node:http";
import { writeFileSync } from "node:fs";

const port = Number.parseInt(process.argv[2] ?? "", 10);
const evidencePath = process.argv[3];
const maxRequestBytes = 2 * 1024 * 1024;
const toolMarker = "WAYFINDER_TOOL_ROUNDTRIP";
const finalMarker = "WAYFINDER_CODEX_SMOKE_OK";

if (!Number.isInteger(port) || port < 1024 || port > 65535 || !evidencePath) {
  process.stderr.write("usage: node codex-mock-provider.mjs PORT EVIDENCE_PATH\n");
  process.exit(2);
}

let requestCount = 0;

function json(response, status, value) {
  const body = JSON.stringify(value);
  response.writeHead(status, {
    "content-type": "application/json",
    "content-length": Buffer.byteLength(body),
  });
  response.end(body);
}

function sse(response, chunks) {
  response.writeHead(200, {
    "content-type": "text/event-stream",
    "cache-control": "no-cache",
    connection: "keep-alive",
  });
  for (const chunk of chunks) {
    response.write(`data: ${JSON.stringify(chunk)}\n\n`);
  }
  response.end("data: [DONE]\n\n");
}

function functionTools(body) {
  return (Array.isArray(body.tools) ? body.tools : [])
    .filter((tool) => tool?.type === "function" && tool.function?.name)
    .map((tool) => tool.function);
}

function execTool(body) {
  const tools = functionTools(body);
  return tools.find((tool) => tool.name === "exec_command")
    ?? tools.find((tool) => tool.name.includes("exec_command"))
    ?? tools.find((tool) => tool.parameters?.properties?.cmd);
}

function toolResult(body) {
  return (Array.isArray(body.messages) ? body.messages : [])
    .find((message) => message?.role === "tool" && message.tool_call_id === "call_wayfinder_smoke");
}

const server = createServer((request, response) => {
  if (request.method === "GET" && request.url === "/healthz") {
    return json(response, 200, { status: "ok" });
  }
  if (request.method !== "POST" || request.url !== "/v1/chat/completions") {
    return json(response, 404, { error: { message: "not found" } });
  }

  let size = 0;
  const chunks = [];
  request.on("data", (chunk) => {
    size += chunk.length;
    if (size > maxRequestBytes) {
      request.destroy();
      return;
    }
    chunks.push(chunk);
  });
  request.on("end", () => {
    let body;
    try {
      body = JSON.parse(Buffer.concat(chunks).toString("utf8"));
    } catch {
      return json(response, 400, { error: { message: "invalid JSON" } });
    }
    if (body.model !== "smoke-model" || body.stream !== true) {
      return json(response, 400, { error: { message: "unexpected model or stream mode" } });
    }

    requestCount += 1;
    const result = toolResult(body);
    if (!result) {
      const tool = execTool(body);
      if (!tool) {
        return json(response, 400, { error: { message: "Codex exec_command tool was not translated" } });
      }
      return sse(response, [
        {
          id: "chatcmpl-wayfinder-smoke-1",
          object: "chat.completion.chunk",
          model: "smoke-model",
          choices: [{
            index: 0,
            delta: {
              tool_calls: [{
                index: 0,
                id: "call_wayfinder_smoke",
                type: "function",
                function: {
                  name: tool.name,
                  arguments: JSON.stringify({ cmd: `printf ${toolMarker}` }),
                },
              }],
            },
            finish_reason: "tool_calls",
          }],
          usage: { prompt_tokens: 32, completion_tokens: 8 },
        },
      ]);
    }

    const content = typeof result.content === "string" ? result.content : JSON.stringify(result.content);
    if (!content.includes(toolMarker)) {
      return json(response, 400, { error: { message: "tool result marker did not return through Wayfinder" } });
    }
    writeFileSync(evidencePath, `${JSON.stringify({
      schema_version: 1,
      codex_contract: "0.149.0",
      requests: requestCount,
      tool_name: "exec_command",
      tool_call_id: "call_wayfinder_smoke",
      tool_output_seen: true,
      final_marker: finalMarker,
    }, null, 2)}\n`, { mode: 0o600 });
    return sse(response, [
      {
        id: "chatcmpl-wayfinder-smoke-2",
        object: "chat.completion.chunk",
        model: "smoke-model",
        choices: [{
          index: 0,
          delta: { content: finalMarker },
          finish_reason: "stop",
        }],
        usage: { prompt_tokens: 48, completion_tokens: 6 },
      },
    ]);
  });
});

server.listen(port, "127.0.0.1", () => {
  process.stdout.write(`Codex smoke provider listening on 127.0.0.1:${port}\n`);
});
