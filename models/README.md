# Local model

IntentClip currently uses `Qwen3-0.6B-Q8_0.gguf` for local intent classification.

- Model: Qwen3-0.6B-GGUF (Q8_0)
- Publisher: Qwen
- Source: https://modelscope.cn/models/Qwen/Qwen3-0.6B-GGUF
- Canonical model card: https://huggingface.co/Qwen/Qwen3-0.6B-GGUF
- License: Apache-2.0
- Expected size: 639,446,688 bytes
- SHA-256: `9465e63a22add5354d9bb4b99e90117043c7124007664907259bd16d043bb031`

The GGUF file is intentionally excluded from Git. Place it at:

```text
models/Qwen3-0.6B-Q8_0.gguf
```

For packaged or custom deployments, set `INTENTCLIP_MODEL_PATH` to an absolute GGUF path before starting IntentClip.

There is no official model named `Qwen3.5-0.6B`; the official 0.6B model is Qwen3-0.6B.
