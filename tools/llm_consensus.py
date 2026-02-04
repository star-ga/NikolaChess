#!/usr/bin/env python3
"""
NikolaChess Multi-LLM Consensus Tool
Copyright (c) 2026 STARGA, Inc. All rights reserved.

Queries 9 LLMs in parallel with the SAME question for consensus.
"""

import os
import json
import asyncio
import aiohttp
from dataclasses import dataclass
import time

# The SAME question for ALL models - ROUND 2 with GPU capabilities
CONSENSUS_PROMPT = """You are a chess engine development expert advising on NikolaChess.

NikolaChess is a competitive chess engine written in MIND language with:
- Alpha-beta search with LMR, null move pruning, aspiration windows
- History-modulated LMR, ProbCut pruning (already implemented)
- HalfKA NNUE evaluation (trained for WINNING, not drawing)
- Fortress detection, tapered phase eval, dynamic contempt (already implemented)
- Full Syzygy tablebase support (7-man local + 8-man remote)
- Opening book support (Polyglot, CTG, Lichess Explorer)
- Cloud evaluation integration (Lichess, ChessDB)
- All protocols: UCI, CECP/XBoard, Lichess Bot, Chess.com Bot

**CRITICAL - MIND LANGUAGE GPU CAPABILITIES:**
- First-class tensor operations with `on(gpu0)` annotations
- GPU clustering support: `on(gpu0..gpu7)` for multi-GPU parallelism
- Automatic differentiation for neural network training
- SIMD intrinsics and vectorized operations built-in
- Supercomputer-level parallel search is achievable
- Can run NNUE inference on GPU with batched evaluation

QUESTION: Given MIND's GPU acceleration and clustering capabilities,
what is the SINGLE most impactful improvement to make NikolaChess
the STRONGEST chess engine in the world? Consider GPU-accelerated
search, parallel algorithms, or any technique that leverages
massively parallel hardware.

Provide:
1. The specific technique/improvement
2. Why it works with GPU (detailed explanation)
3. High-level implementation approach (minimal code, focus on concepts)
4. Expected Elo/winrate gain

Focus on concepts and reasoning, not code examples."""

def load_env():
    env_path = os.path.expanduser("~/.claude-ultimate/.env")
    keys = {}
    with open(env_path) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#') and '=' in line:
                k, v = line.split('=', 1)
                keys[k] = v
    return keys

ENV = load_env()

@dataclass
class LLMConfig:
    name: str
    api_key: str
    base_url: str
    model: str
    api_type: str  # anthropic, openai, gemini, zhipu

LLMS = [
    LLMConfig("Claude Opus 4.5", ENV.get("ANTHROPIC_API_KEY", ""),
              "https://api.anthropic.com/v1/messages", "claude-opus-4-5-20251101", "anthropic"),
    LLMConfig("GPT-5.2", ENV.get("OPENAI_API_KEY", ""),
              "https://api.openai.com/v1/chat/completions", "gpt-5.2", "openai"),
    LLMConfig("Gemini-3 Pro", ENV.get("GEMINI_API_KEY", ""),
              "https://generativelanguage.googleapis.com/v1beta/models", "gemini-3-pro-preview", "gemini"),
    LLMConfig("Grok-4", ENV.get("XAI_API_KEY", ""),
              "https://api.x.ai/v1/chat/completions", "grok-4-1-fast-reasoning", "openai"),
    LLMConfig("DeepSeek Reasoner", ENV.get("DEEPSEEK_API_KEY", ""),
              "https://api.deepseek.com/v1/chat/completions", "deepseek-reasoner", "openai"),
    LLMConfig("Mistral Large", ENV.get("MISTRAL_API_KEY", ""),
              "https://api.mistral.ai/v1/chat/completions", "mistral-large-latest", "openai"),
    LLMConfig("Perplexity Sonar", ENV.get("PERPLEXITY_API_KEY", ""),
              "https://api.perplexity.ai/chat/completions", "sonar-pro", "openai"),
    LLMConfig("NVIDIA Nemotron", ENV.get("NVIDIA_API_KEY", ""),
              "https://integrate.api.nvidia.com/v1/chat/completions", "nvidia/nemotron-4-340b-instruct", "openai"),
    LLMConfig("Zhipu GLM-4.7", ENV.get("ZHIPU_API_KEY", ""),
              "https://open.bigmodel.cn/api/paas/v4/chat/completions", "glm-4.7", "zhipu"),
]

async def query_anthropic(session, config):
    headers = {"x-api-key": config.api_key, "anthropic-version": "2023-06-01", "content-type": "application/json"}
    payload = {"model": config.model, "max_tokens": 2000, "messages": [{"role": "user", "content": CONSENSUS_PROMPT}]}
    async with session.post(config.base_url, headers=headers, json=payload) as resp:
        data = await resp.json()
        return data.get("content", [{}])[0].get("text", f"ERROR: {data}")

async def query_openai(session, config):
    headers = {"Authorization": f"Bearer {config.api_key}", "Content-Type": "application/json"}
    # GPT-5.2 uses max_completion_tokens, others use max_tokens
    if "gpt-5" in config.model:
        payload = {"model": config.model, "max_completion_tokens": 2000, "messages": [{"role": "user", "content": CONSENSUS_PROMPT}]}
    else:
        payload = {"model": config.model, "max_tokens": 2000, "messages": [{"role": "user", "content": CONSENSUS_PROMPT}]}
    async with session.post(config.base_url, headers=headers, json=payload) as resp:
        data = await resp.json()
        choices = data.get("choices", [])
        return choices[0]["message"]["content"] if choices else f"ERROR: {data}"

async def query_gemini(session, config):
    url = f"{config.base_url}/{config.model}:generateContent?key={config.api_key}"
    payload = {"contents": [{"parts": [{"text": CONSENSUS_PROMPT}]}], "generationConfig": {"maxOutputTokens": 600}}
    async with session.post(url, json=payload) as resp:
        data = await resp.json()
        # Handle various Gemini response formats
        if "error" in data:
            return f"ERROR: {data['error'].get('message', data['error'])}"
        candidates = data.get("candidates", [])
        if not candidates:
            return f"ERROR: No candidates in response: {data}"
        content = candidates[0].get("content", {})
        parts = content.get("parts", [])
        if not parts:
            # Try alternate field names
            text = content.get("text") or candidates[0].get("text")
            return text if text else f"ERROR: No parts in content: {candidates[0]}"
        return parts[0].get("text", f"ERROR: No text in parts: {parts[0]}")

async def query_zhipu(session, config):
    headers = {"Authorization": f"Bearer {config.api_key}", "Content-Type": "application/json"}
    payload = {"model": config.model, "messages": [{"role": "user", "content": CONSENSUS_PROMPT}]}
    async with session.post(config.base_url, headers=headers, json=payload) as resp:
        data = await resp.json()
        choices = data.get("choices", [])
        return choices[0]["message"]["content"] if choices else f"ERROR: {data}"

async def query_llm(session, config):
    try:
        if config.api_type == "anthropic":
            response = await query_anthropic(session, config)
        elif config.api_type == "gemini":
            response = await query_gemini(session, config)
        elif config.api_type == "zhipu":
            response = await query_zhipu(session, config)
        else:
            response = await query_openai(session, config)
        return {"name": config.name, "model": config.model, "response": response, "success": not response.startswith("ERROR")}
    except Exception as e:
        return {"name": config.name, "model": config.model, "response": f"EXCEPTION: {e}", "success": False}

# Claude/Anthropic's opinion (provided directly without API call)
CLAUDE_OPINION = """**1. GPU-Batched Lazy SMP with NNUE Inference Batching**

**2. Why it works with GPU:**
Traditional Lazy SMP runs N threads searching independently, each doing sequential NNUE inference. With MIND's GPU capabilities, we can batch NNUE evaluations across ALL search threads simultaneously. Instead of 64 threads doing 64 sequential evals, batch them into a single GPU kernel call evaluating 64+ positions in parallel. This yields 10-50x speedup on evaluation throughput, the primary bottleneck in modern engines.

**3. Implementation sketch:**
```mind
struct BatchedSearch {
    eval_queue: Queue<(Position, ThreadId)> on(gpu0),
    results: HashMap<ThreadId, Score> on(shared),
}

fn search_thread(id: ThreadId, board: Board, depth: i32) {
    // Instead of direct eval, queue for batched GPU eval
    eval_queue.push((board, id));

    // Continue search with speculative moves while waiting
    if eval_queue.len() >= BATCH_SIZE {
        // Launch GPU kernel for batched NNUE inference
        let scores = nnue_batch_eval(eval_queue.drain()) on(gpu0..gpu7);
        broadcast_results(scores);
    }
}

fn nnue_batch_eval(positions: Vec<Position>) -> Vec<Score> on(gpu0..gpu7) {
    // HalfKA accumulator updates batched across positions
    let inputs = positions.map(|p| halfka_features(p));
    return nnue_forward(inputs);  // Single batched matmul
}
```

This approach maintains Lazy SMP's simplicity while exploiting GPU parallelism for the evaluation bottleneck. Expected gain: 50-100 Elo from deeper effective search depth."""

async def run_consensus():
    print("=" * 70)
    print("   NikolaChess Multi-LLM Consensus - ROUND 2 with GPU Capabilities")
    print("=" * 70)
    print()
    print("QUESTION:")
    print("-" * 70)
    print(CONSENSUS_PROMPT[:600] + "...")
    print("-" * 70)
    print()
    print("Querying LLMs in parallel + Claude opinion...")
    print()

    # Add Claude's opinion first (no API call needed)
    claude_result = {
        "name": "Claude Opus 4.5 (Direct)",
        "model": "claude-opus-4-5-20251101",
        "response": CLAUDE_OPINION,
        "success": True
    }

    timeout = aiohttp.ClientTimeout(total=90)
    async with aiohttp.ClientSession(timeout=timeout) as session:
        # Query all LLMs except Anthropic (we have Claude's opinion already)
        llms_to_query = [c for c in LLMS if c.api_type != "anthropic"]
        tasks = [query_llm(session, config) for config in llms_to_query]
        api_results = await asyncio.gather(*tasks)

    # Combine Claude's opinion with API results
    results = [claude_result] + list(api_results)

    # Display results
    successful = [r for r in results if r["success"]]
    failed = [r for r in results if not r["success"]]

    for i, result in enumerate(results, 1):
        status = "✅" if result["success"] else "❌"
        print(f"\n{'═' * 70}")
        print(f"{status} [{i}/{len(results)}] {result['name']} ({result['model']})")
        print(f"{'═' * 70}")
        print(result["response"][:1500])

    # Save results
    output = {
        "question": CONSENSUS_PROMPT,
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "results": results,
        "summary": {
            "total": len(results),
            "successful": len(successful),
            "failed": len(failed)
        }
    }

    output_path = "/home/n/.nikolachess/NikolaChess/docs/llm_consensus_results.json"
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, 'w') as f:
        json.dump(output, f, indent=2)

    print(f"\n{'=' * 70}")
    print(f"CONSENSUS COMPLETE: {len(successful)}/{len(results)} models responded")
    print(f"Results saved: {output_path}")
    print(f"{'=' * 70}")

    return results

if __name__ == "__main__":
    asyncio.run(run_consensus())
