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
- ⏳ Ainda faltam: contraste final de estados pressionados, invalidação em sub-regiões por componente e limpeza opcional de caminhos legados.

### Tarefa pendente assumida nesta rodada
- ✅ **Revisão de contraste/estado pressionado em ações críticas** para melhorar leitura em uso real.
  - `UiButton` agora usa paleta de `pressed` por variante (`Primary`, `Secondary`, `Danger`) em vez de um único preenchimento genérico.
  - Estado `disabled` passou a usar texto secundário para diferenciar melhor ações indisponíveis.
  - Tela `Perform` destaca `MUTE/UNMUTE` com variante `Danger` quando a trilha ativa está mutada.

### O que resta até agora
1. Concluir revisão visual completa de contraste/press state em **todas** as telas (a rodada atual cobriu botões; ainda falta validar `UiChip`, `UiMacroRow` e `UiFader` em campo).
2. Refinar invalidação para blocos internos (ex.: redesenhar só `UiMacroRow` alterada em vez do painel inteiro).
3. Decidir escopo de limpeza de legado (`LegacyRender`/rotas antigas) para reduzir manutenção duplicada sem perder rollback rápido.
