# OpenCode miniDBMS team

The project configuration makes `architect` the default primary agent. It uses `openai/gpt-5.6-sol` and can delegate only to the `implementer` subagent, which uses the official DeepSeek provider model `deepseek/deepseek-v4-flash`.

Before the first run, start OpenCode and use `/connect` to configure credentials for the OpenAI and DeepSeek providers. API keys stay in OpenCode's user credential store and must not be committed to this repository.

Run `opencode` from the repository root and give the task to `architect`. Plans are written to `docs/plans/`; that directory is covered by the repository's existing `docs/*` ignore rule because `docs/` is also Doxygen output.

After changing any file in `opencode.json` or `.opencode/`, quit and restart existing OpenCode sessions because configuration is loaded only at startup.

Useful configuration checks:

```sh
opencode debug config
opencode debug agent architect
opencode debug agent implementer
opencode debug skill
```
