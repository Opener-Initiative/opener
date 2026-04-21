# Contributing to Opener

## Use of AI assistants in contributions

Large Language Models (LLMs) and AI-based coding tools have become part of many developers' everyday workflows. The *Opener Initiative* does not prohibit their use, but we ask contributors to apply them thoughtfully and within the expectations described below.

The following three considerations shape our policy:

1. **Legal integrity:** *Opener* is explicitly intended for integration into commercial products and released under Apache 2.0, which requires contributors to have the right to license their contributions. AI coding assistants can, in rare cases, reproduce material from training data under incompatible licenses. Moreover, there's ongoing legal ambiguity about the copyright status of purely AI-generated output. It's important to keep the codebase cleanly licensable.
2. **Technical correctness:** LLMs likely have had limited exposure to ETSI TS 103 636 during training and tend to produce output that sounds plausible but misinterprets the specification in subtle ways. Our goal of passing ETSI-defined conformance tests leaves no room for such errors to slip through.
3. **Accountability:** Reviewers and adopters of the codebase need to trust that every contribution has been understood and vouched for by a human contributor who stands behind it.

Following from these considerations, our policy is built around the principle that human contributors are fully responsible for everything they submit, regardless of how it was produced. By opening a pull request you confirm that:

- You understand the code you are submitting and can explain and defend its design during review.
- You have the right to contribute it under Apache 2.0, and you have taken reasonable steps to ensure it does not incorporate material from sources with incompatible licenses.
- You have reviewed the output of any AI tool you used, rather than pasting it unchanged.
- If a substantial portion of the contribution was drafted with AI assistance, a brief note in the pull request description is appreciated.

For non-trivial protocol logic, we expect clear references to the relevant sections of ETSI TS 103 636, adequate tests, and a rationale the contributor can stand behind. This is the most effective safeguard against specification misinterpretations, whether introduced by humans or by AI.

Major design decisions are made through open discussion in issues or community meetings, with the rationale documented. AI tools may support individual contributors in thinking through options, but the decisions themselves are made through human deliberation within the community.

To summarize: Be transparent, take responsibility for your contribution, and hold AI-assisted work to the same standard as anything else you would submit. If you are unsure whether a particular use of AI assistance fits within these expectations, open an issue or raise it in the relevant discussion channel before starting significant work — we would rather clarify early than rework late.
