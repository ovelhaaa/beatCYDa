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
Avançar na pendência de Sprint 7 de **revisão de contraste e estado pressionado** em componentes ainda sem tratamento visual consistente.

### O que foi implementado

1. `UiChip` recebeu contraste semântico por estado:
   - borda ativa/selecionada com `Accent`.
   - texto em estados ativos usando `TextOnAccent`.
2. `UiMacroRow` ganhou estados de pressão explícitos por controle (`minusPressed`/`plusPressed`) e cores dedicadas quando segurado no hold-repeat.
3. `PatternScreen` e `SoundScreen` passaram a propagar o estado real de hold para renderizar `+/-` pressionado durante o toque contínuo.
4. `UiFader` agora destaca o rótulo do canal com `AccentPressed` + `TextOnAccent` quando capturado.
5. `UiModal` deixou de usar cor mágica no scrim e passou a usar token (`ModalScrim`) + diferenciação de tipografia do corpo (`TextSecondary`).

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
- ✅ **Refino de contraste e pressed state** em componentes pendentes.
  - `UiChip`, `UiMacroRow`, `UiFader` e `UiModal` foram ajustados para usar tokens semânticos de contraste.
  - `PatternScreen` e `SoundScreen` agora expõem visual de pressão real no `+/-` durante hold-repeat.

### Atualização incremental (rodada atual)
- ✅ Implementado primeiro passo de padronização de contraste/feedback tátil:
  - chips de trilha e rótulos de fader agora reforçam estado ativo/capturado por token semântico.
  - controles `+/-` de macro rows exibem estado pressionado durante hold-repeat.
  - modal usa token de scrim e hierarquia de texto mais legível.

### O que resta até agora
1. Validar em hardware real o contraste final dos componentes revisados (`UiChip`, `UiMacroRow`, `UiFader`, `UiModal`) sob iluminação forte.
2. Avançar de invalidação por **tela** para invalidação por **componente/retângulo** (ex.: redraw seletivo de `UiMacroRow` e cards).
3. Consolidar hardcodes de timing remanescentes em `CYD_Config.h`.
4. Decidir escopo de limpeza de legado (`LegacyRender`/rotas antigas) para reduzir manutenção duplicada sem perder rollback rápido.

### Restante atualizado
1. Implementar dirty-rect por componente nas telas com maior custo de redraw (Pattern e Sound primeiro).
2. Validar contraste/legibilidade em dispositivo real e ajustar tokens finais de estado pressionado se necessário.
3. Consolidar tempos/thresholds dispersos em `CYD_Config.h`.
4. Definir janela de remoção/encapsulamento do pipeline legado mantendo rollback via flag em um commit.
