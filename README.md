# Lagostom Glue

Plugin de mixagem (VST3 / AU) da Lagostom Productions: saturação + compressor "glue" em um só plugin.

## Cadeia de sinal

```
Entrada → Drive → Saturação (tanh, 2x oversampling) → DC blocker → Tone → Compressor → Output → Mix → Saída
              └──────────────── cópia do sinal original (dry, com latência compensada) ───────────────────┘
```

| Parâmetro | O que faz |
|---|---|
| Drive | Ganho antes da saturação. Mais drive = mais harmônicos |
| Warmth | Assimetria da curva: adiciona harmônicos pares (som mais "quente") |
| Tone | Filtro passa-baixa depois da saturação, tira o brilho áspero |
| Threshold / Ratio / Attack / Release | Compressor que "cola" o som |
| Output | Volume de saída (compensa o ganho do drive) |
| Mix | Mistura sinal original e processado (compressão paralela) |

## Como um plugin de áudio funciona

1. A DAW carrega o arquivo `.vst3` (uma DLL com uma interface padronizada).
2. Ela chama `prepareToPlay(sampleRate, blockSize)` uma vez: é aqui que se aloca memória e se configura o DSP.
3. Durante a reprodução, chama `processBlock(buffer)` centenas de vezes por segundo. O `buffer` guarda um bloco de amostras (ex.: 512) por canal, e o plugin modifica essas amostras no lugar.
4. Os parâmetros ficam em `AudioProcessorValueTreeState`, que conversa com a automação da DAW de forma segura entre threads.
5. O `processBlock` roda na thread de áudio em tempo real: sem alocar memória, sem travar, sem ler arquivo.

## Como compilar (sem instalar nada)

1. Crie um repositório no GitHub e envie esta pasta.
2. Na aba **Actions**, o workflow "Build plugin" compila sozinho.
3. Baixe o artefato `LagostomGlue-VST3-windows` e copie a pasta `Lagostom Glue.vst3` para `C:\Program Files\Common Files\VST3`.
4. No FL Studio: Options → Manage plugins → Find plugins.

## Compilar localmente

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
