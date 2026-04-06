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

### Atualização incremental (2026-04-05 — verificação + hardcodes de layout)
- ✅ **Verificação do estado real da implementação** concluída contra o spec:
  - Pipeline novo (`UiApp`) ativo por feature flag, com 5 telas navegáveis.
  - Sprints 1–6 seguem entregues; Sprint 7 permanece com foco em invalidação fina e validação visual final em hardware.
- ✅ **Tarefa pendente assumida**: reduzir hardcodes visuais de layout no shell da UI.
  - `UiMetrics` recebeu novos tokens semânticos para top bar e botões de navegação inferior.
  - `UiApp` migrou coordenadas/tamanhos hardcoded da top bar/nav para os tokens de `UiMetrics`.
  - Resultado: manutenção mais previsível e menor risco de drift visual entre telas/rodadas.

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

### Atualização incremental (2026-04-06 — invalidação interna em PatternScreen)
- ✅ **Tarefa pendente assumida**: iniciar invalidação por sub-regiões/componentes em tela de maior custo (`PatternScreen`).
- ✅ **Implementado em código**:
  - `PatternScreen` passou a renderizar por blocos (`preview`, chips de trilha, macro rows), evitando redraw total do painel a cada alteração.
  - Re-render de botões de ação (`RANDOM`/`CLEAR`) mantido apenas no ciclo de render completo.
  - Comparações de estado locais foram adicionadas para detectar mudanças relevantes por seção (seleção de trilha, mute, padrões e hold state).
  - Correção de estado pressionado no hold-repeat: ao soltar o toque, a linha volta ao estado visual neutro no frame seguinte.
- ⏳ **Próximos passos após esta rodada**:
  1. Replicar estratégia de invalidação interna no `SoundScreen` (segundo maior alvo de macro rows).
  2. Validar em hardware real se houve redução perceptível de flicker/custo de redraw em Pattern.
  3. Evoluir para granularidade de sub-região também em componentes de preview/ring, quando aplicável.

### Atualização incremental (2026-04-06 — invalidação interna em SoundScreen)
- ✅ **Tarefa pendente assumida**: replicar invalidação por sub-regiões no `SoundScreen`.
- ✅ **Implementado em código**:
  - `SoundScreen` migrou de redraw completo do conteúdo para redraw seletivo por blocos (`identity card`, chips de trilha e macro rows).
  - Comparação com snapshot anterior foi adicionada para detectar mudanças relevantes (`activeTrack`, `trackMutes` e parâmetros `pitch/decay/timbre/drive`).
  - Estado de hold (`_holdRow/_holdDirection`) passou a participar da invalidação para atualizar feedback visual de botão pressionado sem repaint total da tela.
  - No `justReleased`, o fim do hold agora marca dirty do bloco de rows para retorno imediato ao estado visual neutro.
- ⏳ **Próximos passos após esta rodada**:
  1. Validar em hardware real ganho de fluidez/redução de redraw no `SoundScreen`.
  2. Avançar invalidação em granularidade menor (sub-retângulos por macro row) nas telas Pattern/Sound.
  3. Consolidar validação final de contraste e estados pressionados sob luz forte.

### Atualização incremental (2026-04-06 — sub-regiões por macro row em SoundScreen)
- ✅ **Tarefa pendente assumida**: iniciar granularidade de invalidação em nível de `UiMacroRow` no `SoundScreen`.
- ✅ **Implementado em código**:
  - O redraw das macro rows deixou de limpar o bloco inteiro (`12x102..296x94`) e passou a limpar/desenhar apenas as linhas realmente alteradas.
  - Cada row agora calcula `dirty` individual por mudança de valor (`pitch/decay/timbre/drive`) e por transição de estado visual (`focus`, `minusPressed`, `plusPressed`).
  - Em troca de trilha ativa (`activeTrack`), as 4 rows continuam sendo invalidadas em conjunto para manter consistência visual imediata.
- ⏳ **Próximos passos após esta rodada**:
  1. Aplicar a mesma granularidade por row na `PatternScreen` (atualmente por bloco de rows).
  2. Validar em hardware real redução de redraw/flicker nas interações de hold no `SoundScreen`.
  3. Evoluir para dirty-rect ainda menor em componentes internos da row (value/bar/botões), caso necessário.

### Atualização incremental (2026-04-06 — sub-regiões por macro row em PatternScreen)
- ✅ **Verificação do estado atual**:
  - `PatternScreen` já tinha invalidação por blocos (`preview`, `chips` e bloco único de macro rows), mas ainda repintava todas as rows ao alterar apenas uma linha.
  - O restante das telas segue funcional com navegação integrada via `UiApp`.
- ✅ **Tarefa pendente assumida**: aplicar granularidade de invalidação por `UiMacroRow` também em `PatternScreen`.
- ✅ **Implementado em código**:
  - O redraw das rows deixou de limpar a área inteira (`12x128..296x70`) e passou a limpar/renderizar somente as rows marcadas como dirty.
  - Cada row agora calcula dirty individual por mudança de valor (`steps`, `hits`, `rotate`, `gain`) e por transição visual de hold (`focus`, `minusPressed`, `plusPressed`).
  - Em troca de trilha ativa, as 4 rows continuam sendo invalidadas em conjunto para consistência imediata.
- ⏳ **O que resta até agora**:
  1. Validar em hardware real o ganho de fluidez e redução de flicker no `PatternScreen` durante hold-repeat.
  2. Avaliar granularidade ainda mais fina em cada row (sub-retângulos internos de label/valor/barra/botões), se necessário.
  3. Fechar validação final de contraste/estados pressionados sob iluminação forte.
  4. Definir recorte final da limpeza opcional do pipeline legado mantendo rollback por feature flag.
