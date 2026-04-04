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

- ✅ `PatternScreen`: botão `RANDOM` habilitado e conectado a ação dedicada (`RANDOMIZE_TRACK`).
- ✅ `Engine::handleUiAction`: nova ação gera combinação aleatória de `hits` e `rotation` respeitando limites da trilha e recalcula o pattern com lock de segurança.
- ✅ Estado geral revalidado: navegação/telas principais permanecem integradas no `UiApp` e a pendência principal segue em polimento visual/invalidação mais granular.
