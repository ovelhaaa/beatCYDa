# Avaliação de progresso das telas (beatCYDa)

Este documento consolida o estado **real do código** da nova UI (`UiApp`) e registra a tarefa de Sprint 7 assumida nesta rodada.

## Resumo executivo (verificado em código)

| Tela | Estado | Progresso estimado | Observações |
|---|---|---:|---|
| Perform | Implementada e navegável | 92% | Play/mute/seleção de trilha funcionando e integrados ao dispatcher. |
| Pattern | Implementada e navegável | 88% | `steps/hits/rotate/gain` com hold-repeat + `RANDOM` funcional por trilha. |
| Sound | Implementada e navegável | 88% | `pitch/decay/timbre/drive` com hold-repeat. |
| Mix | Implementada e navegável | 86% | Faders com captura + drag contínuo + hitbox expandido. |
| Project | Implementada e navegável | 85% | Grid de slots + load/save/delete com modal e toast. |

## Verificação de status vs spec

- ✅ Sprints 1 a 6 estão implementadas no código (fundação, componentes, telas e fluxos principais).
- ✅ Métricas de UI já existem no topo (`FPS` + heap livre), cobrindo parte do polimento.
- ✅ Tempos de toast/refresh centralizados em `CYD_Config.h`.
- ⏳ Pendências principais remanescentes: invalidação fina por região, contraste/estados visuais finais e limpeza opcional do legado.

## Tarefa assumida nesta rodada (Sprint 7)

### Objetivo
Avançar na pendência de Sprint 7 de **revisão de contraste/estado pressionado** nos componentes que ainda não tinham refinamento visual final.

### O que foi implementado

1. `UiChip` recebeu estado explícito de `pressed`, com variação de preenchimento em inativo/ativo sem perda de contraste.
2. `UiMacroRow` passou a destacar foco com borda semântica em `Accent`, incluindo a barra de valor.
3. `UiFader` passou a explicitar fundo da canaleta e borda destacada durante captura.
4. `UiModal` ganhou cabeçalho em `Accent` com `TextOnAccent`, reforçando hierarquia visual e legibilidade.

## Checklist do que foi feito até agora (UI nova)

- [x] Feature flag e coexistência legado/novo
- [x] Fundação (`LgfxDisplay`, theme tokens, `UiApp`)
- [x] Componentes base (`UiButton`, `UiChip`, `UiCard`, `UiMacroRow`, `UiToast`, `UiModal`, `UiFader`)
- [x] PerformScreen funcional
- [x] PatternScreen funcional (com hold-repeat)
- [x] SoundScreen funcional (com hold-repeat)
- [x] MixScreen funcional (captura + drag)
- [x] ProjectScreen funcional (save/load/delete + confirmação)
- [x] Invalidação parcial inicial por regiões na `UiApp`
- [~] Revisão final de contraste/estados pressionados em todas as telas
- [ ] Refinar invalidação para sub-regiões internas por tela (ex.: redraw parcial de componente)
- [ ] Limpeza final do legado opcional


## Atualização desta rodada

### Verificação rápida do estado atual
- ✅ O pipeline novo (`UiApp`) segue ativo por feature flag e cobre as 5 telas do spec com navegação inferior integrada.
- ✅ O polimento de Sprint 7 já tem base funcional de redraw parcial (top/content/nav) e métricas de `FPS/heap` no topo.
- ✅ O detector de mudanças do modelo ficou mais seletivo por contexto de tela (inclusive `patterns[track][step]`), evitando redraw de painel em alterações irrelevantes para a tela ativa.
- ⏳ Ainda faltam: contraste final de estados pressionados em validação de hardware, invalidação em sub-regiões por componente e limpeza opcional de caminhos legados.

### Tarefa pendente assumida nesta rodada
- ✅ **Avançar revisão de contraste/pressed state dos componentes pendentes**.
  - Ajustes aplicados em `UiChip`, `UiMacroRow`, `UiFader` e `UiModal`, reduzindo ambiguidades visuais em estado ativo/focado/capturado.

### Atualização incremental (rodada atual)
- ✅ Implementado passo de polimento visual:
  - `UiChip`: novo estado `pressed` com variação de preenchimento em chips ativos e inativos.
  - `UiMacroRow`: borda de foco em `Accent` e barra sincronizada com estado focado.
  - `UiFader`: canaleta com fundo explícito e borda destacada durante captura.
  - `UiModal`: cabeçalho de título em `Accent` com `TextOnAccent` para contraste consistente.

### O que resta até agora
1. Validar em hardware real o contraste final dos componentes revisados (`UiChip`, `UiMacroRow`, `UiFader`, `UiModal`) sob iluminação forte.
2. Avançar de invalidação por **tela** para invalidação por **componente/retângulo** (ex.: redraw seletivo de `UiMacroRow` e cards).
3. Consolidar eventuais hardcodes remanescentes fora do fluxo principal da UI (rotas legadas auxiliares).
4. Decidir escopo de limpeza de legado (`LegacyRender`/rotas antigas) para reduzir manutenção duplicada sem perder rollback rápido.

### Restante atualizado
1. Implementar dirty-rect por componente nas telas com maior custo de redraw (Pattern e Sound primeiro).
2. Validar contraste/legibilidade em dispositivo real e ajustar tokens finais de estado pressionado se necessário.
3. Revisar se ainda existem timings hardcoded fora da UI principal (ex.: caminhos legados auxiliares).
4. Definir janela de remoção/encapsulamento do pipeline legado mantendo rollback via flag em um commit.
