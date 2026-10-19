These tiny fixtures derive from libailo's own generated solid-red image and
synthetic 440 Hz sine wave. They carry the project's MIT license. No model or
third-party media is embedded here.

They were generated once with host ffmpeg; ffmpeg is not a library, build or
runtime dependency of libailo. Reproduce after running `tests/media_fixture.py`:

```sh
ffmpeg -i red.png -vf scale=32:32 -frames:v 1 -threads 1 -update 1 red.jpg
ffmpeg -i tone.wav -t 0.1 -threads 1 -c:a libmp3lame -b:a 64k tone.mp3
ffmpeg -i tone.wav -t 0.1 -threads 1 -c:a flac -metadata encoder=libailo-fixture tone.flac
```

The committed bytes are the test inputs; encoder versions can change output
metadata and byte identity. SHA-256:

- `red.jpg`: `7d80e1b1d656b4247d4975ebccac09caf7527b91c330c093bb1f9ea41ca4e44b`
- `tone.mp3`: `257c6fa79680aa87657fafec17657b9e1c332f2b7eef6a8d84be32e4371dbedb`
- `tone.flac`: `9018b5d2ea93696df935475ee0c24ea245ddaf1f1f17cb48fa4593137e36a07b`
