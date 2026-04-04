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
Implementar **invalidação parcial por regiões** na `UiApp` para evitar redraw completo desnecessário a cada frame.

### O que foi implementado

1. `UiApp` passou a manter estado de invalidação com `UiInvalidation`.
2. Pipeline de frame foi separado por regiões:
   - top bar (`renderChrome`) só redesenha quando suja.
   - conteúdo da tela ativa só redesenha quando sujo.
   - bottom nav só redesenha quando suja.
3. Foi adicionado detecção de mudanças no snapshot (`detectModelChanges`) para invalidar apenas quando o modelo realmente mudou (BPM/play/topo; track/params/conteúdo).
4. Navegação inferior agora marca a região de nav como suja e troca de tela dispara `invalidateAll()`.

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
- [ ] Revisão final de contraste/estados pressionados em todas as telas
- [ ] Refinar invalidação para sub-regiões internas por tela (ex.: redraw parcial de componente)
- [ ] Limpeza final do legado opcional


## Atualização desta rodada

### Verificação rápida do estado atual
- ✅ O pipeline novo (`UiApp`) segue ativo por feature flag e cobre as 5 telas do spec com navegação inferior integrada.
- ✅ O polimento de Sprint 7 já tem base funcional de redraw parcial (top/content/nav) e métricas de `FPS/heap` no topo.
- ✅ O detector de mudanças do modelo ficou mais seletivo por contexto de tela (inclusive `patterns[track][step]`), evitando redraw de painel em alterações irrelevantes para a tela ativa.
- ⏳ Ainda faltam: contraste final de estados pressionados em validação de hardware, invalidação em sub-regiões por componente e limpeza opcional de caminhos legados.

### Tarefa pendente assumida nesta rodada
- ✅ **Refino de invalidação para reduzir redraw desnecessário por tela ativa**.
  - `UiApp::detectModelChanges()` agora decide `panelDirty` com base no contexto da tela (`UiScreenId`) em vez de um gatilho global único.
  - Foi adicionada comparação explícita de dados de padrão (`patternLens` + matriz `patterns`) para capturar mudanças reais vindas do engine/ações.
  - `ProjectScreen` passa a invalidar por mudanças realmente relevantes da própria tela (`bpm` e `activeTrack`), evitando redraw de painel quando parâmetros de mix/sound mudam em background.

### Atualização incremental (rodada atual)
- ✅ Implementado primeiro passo de invalidação de sub-regiões por contexto:
  - O topo (transport/metrics) continua invalidado de forma independente.
  - O conteúdo central passa a responder apenas a mudanças de modelo relevantes para a tela atual.
  - Com isso, alterações de parâmetros fora do contexto de `Project` não forçam redraw do painel dessa tela.

### O que resta até agora
1. Validar em hardware real o contraste final dos componentes revisados (`UiChip`, `UiMacroRow`, `UiFader`) sob iluminação forte.
2. Avançar de invalidação por **tela** para invalidação por **componente/retângulo** (ex.: redraw seletivo de `UiMacroRow` e cards).
3. Decidir escopo de limpeza de legado (`LegacyRender`/rotas antigas) para reduzir manutenção duplicada sem perder rollback rápido.

### Restante atualizado
1. Implementar dirty-rect por componente nas telas com maior custo de redraw (Pattern e Sound primeiro).
2. Validar contraste/legibilidade em dispositivo real e ajustar tokens finais de estado pressionado se necessário.
3. Definir janela de remoção/encapsulamento do pipeline legado mantendo rollback via flag em um commit.
