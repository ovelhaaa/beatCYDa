# beatCYDa — Spec de implementação por arquivo (UI LovyanGFX)

Este documento transforma o plano macro em execução direta por arquivo, com contratos de classe, ordem de implementação e checklist de merge por etapa.

## 1) Escopo e princípios

- Hardware alvo: **ESP32 Cheap Yellow Display 2.8" resistivo (320x240)**.
- Regra de segurança: migrar **sem quebrar áudio/motor rítmico**.
- Estratégia: manter legado funcionando e ativar nova UI por feature flag.

## 2) Feature flag e estratégia de coexistência

### Arquivos
- `src/Config.h`
- `src/main.cpp`
- `src/ui/Display.h`
- `src/ui/Display.cpp`

### Mudanças
1. Adicionar flag de build/runtime:
   - `constexpr bool UseNewUi = true/false;` (build) **ou** seleção por macro em `platformio.ini`.
2. `displayTask()` passa a rotear:
   - `UseNewUi == false` → pipeline legado atual.
   - `UseNewUi == true` → pipeline `UiApp` (novo).
3. Garantir rollback em 1 commit (troca da flag apenas).

### Critério de aceite
- Build com UI legada intacta.
- Build com Nova UI inicializando com tela de smoke test.

### Status atual (implementado)
- ✅ Flag `CYD_USE_NEW_UI` + `UseNewUi` adicionada (`platformio.ini` + `src/Config.h`).
- ✅ `displayTask()` com roteamento legado/novo (`src/ui/Display.cpp`).
- ✅ Rollback em 1 commit via troca da flag (default permanece em UI legada).

---

## 3) Ordem de criação (implementação incremental)

1. **Fundação de display/theme/core**
2. **Componentes base reutilizáveis**
3. **PerformScreen**
4. **PatternScreen + SoundScreen**
5. **MixScreen (drag robusto)**
6. **ProjectScreen (fluxo seguro de storage)**
7. **Polimento (invalidação fina, performance, cleanup legado)**

---

## 4) Especificação por arquivo

## 4.1 Infra de display

### `src/ui/LgfxDisplay.h`
**Responsabilidade:** encapsular LovyanGFX (TFT, touch, backlight, rotação e sprites).

```cpp
#pragma once
#include <LovyanGFX.hpp>

namespace ui {

struct TouchPoint {
  int16_t x{0}, y{0};
  bool pressed{false};
  bool justPressed{false};
  bool justReleased{false};
  bool dragging{false};
};

class LgfxDisplay {
public:
  bool begin();
  void setRotation(uint8_t rot);
  void setBrightness(uint8_t level);

  int16_t width() const;
  int16_t height() const;

  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) const;
  bool readTouch(TouchPoint& tp);

  lgfx::LGFX_Device& canvas();
  LGFX_Sprite createSprite();

private:
  // driver/panel/backlight/touch config local
};

} // namespace ui
```

### `src/ui/LgfxDisplay.cpp`
**Responsabilidade:** implementação do contrato acima e init centralizado.

**Checklist interno:**
- [x] init TFT + touch + backlight
- [x] rotação aplicada
- [x] `readTouch()` mapeando pressed/edges
- [x] sprite de teste operacional

---

## 4.2 Tema (tokens visuais)

### `src/ui/theme/UiColors.h`
**Responsabilidade:** paleta semântica (sem “cor mágica”).

```cpp
namespace ui::theme {
struct UiColors {
  static constexpr uint16_t Bg = 0x0000;
  static constexpr uint16_t Surface = 0x18C3;
  static constexpr uint16_t Outline = 0x39E7;
  static constexpr uint16_t TextPrimary = 0xFFFF;
  static constexpr uint16_t TextSecondary = 0xBDF7;
  static constexpr uint16_t Accent = 0x2D7F;
  static constexpr uint16_t Warning = 0xFD20;
  static constexpr uint16_t Danger = 0xF800;
  static constexpr uint16_t Disabled = 0x630C;
};
}
```

### `src/ui/theme/UiMetrics.h`
**Responsabilidade:** espaçamentos, raios, tamanhos mínimos de toque.

```cpp
namespace ui::theme {
struct UiMetrics {
  static constexpr int ScreenW = 320;
  static constexpr int ScreenH = 240;
  static constexpr int OuterMargin = 8;
  static constexpr int TopBarH = 32;
  static constexpr int BottomNavH = 40;
  static constexpr int RadiusSm = 4;
  static constexpr int RadiusMd = 8;
  static constexpr int TouchMinW = 40;
  static constexpr int TouchMinH = 32;
};
}
```

### `src/ui/theme/UiTypography.h`
**Responsabilidade:** presets de texto.

### `src/ui/theme/UiTheme.h`
**Responsabilidade:** agregador (`colors + metrics + typography`).

**Status atual:** ✅ `UiColors`, `UiMetrics`, `UiTypography` e `UiTheme` criados.

---

## 4.3 Core de estado/eventos/layout

### `src/ui/core/UiScreenId.h`
```cpp
namespace ui {
enum class UiScreenId : uint8_t {
  Perform,
  Pattern,
  Sound,
  Mix,
  Project
};
}
```
**Status atual:** ✅ criado.

### `src/ui/core/UiState.h`
**Responsabilidade:** estado persistente + transitório de UI.

```cpp
namespace ui {
struct UiPersistentState {
  UiScreenId screen{UiScreenId::Perform};
  uint8_t activeTrack{0};
  uint8_t activeSlot{0};
  uint16_t bpm{120};
};

struct UiTransientState {
  int8_t pressedComponent{-1};
  int8_t activeFader{-1};
  bool modalOpen{false};
  bool toastVisible{false};
  uint32_t toastUntilMs{0};
};

struct UiState {
  UiPersistentState persistent;
  UiTransientState transient;
};
}
```

### `src/ui/core/UiEvents.h`
**Responsabilidade:** eventos de navegação/interação.
**Status atual:** ✅ criado (eventos básicos de navegação e touch).

### `src/ui/core/UiInvalidation.h`
**Responsabilidade:** flags de redraw por região.

```cpp
namespace ui {
struct UiInvalidation {
  bool fullScreenDirty{true};
  bool topBarDirty{true};
  bool contentDirty{true};
  bool bottomNavDirty{true};
  bool overlayDirty{true};

  void invalidateAll();
  void clear();
};
}
```
**Status atual:** ✅ `invalidateAll()` adicionado mantendo compatibilidade com `markAll()`.

### `src/ui/core/UiLayout.h`
**Responsabilidade:** retângulos semânticos por tela/região.

---

## 4.4 Ações para engine/control/storage

### `src/ui/core/UiActions.h`
**Responsabilidade:** API única de escrita, sem acesso direto ao engine na tela.

```cpp
namespace ui {
class UiActions {
public:
  void setActiveTrack(uint8_t track);
  void setBpm(uint16_t bpm);
  void setPatternSteps(uint8_t track, uint8_t value);
  void setPatternHits(uint8_t track, uint8_t value);
  void setTrackMute(uint8_t track, bool mute);
  void setTrackVolume(uint8_t track, uint8_t value);

  void saveSlot(uint8_t slot);
  void loadSlot(uint8_t slot);
};
}
```

### `src/ui/core/UiActions.cpp`
**Responsabilidade:** traduzir para `postUiAction(...)`/dispatcher existente.

---

## 4.5 Base de telas e App

### `src/ui/screens/IScreen.h`
**Responsabilidade:** contrato comum de tela.

```cpp
namespace ui {
class IScreen {
public:
  virtual ~IScreen() = default;
  virtual void layout() = 0;
  virtual void render() = 0;
  virtual void handleTouch(const TouchPoint& tp) = 0;
  virtual void invalidate() = 0;
};
}
```

### `src/ui/UiApp.h` / `src/ui/UiApp.cpp`
**Responsabilidade:** orquestrador de frame novo (`input -> update -> render`).

Fluxo recomendado:
1. polling touch
2. roteamento de eventos
3. update de estado/transientes
4. redraw por invalidação
5. overlays por último

**Status atual:** ✅ `UiApp` inicial criado com smoke test de render e leitura de touch.

---

## 4.6 Componentes (reutilizáveis)

### `src/ui/components/UiButton.h/.cpp`
Props:
- `label`, `variant`, `pressed`, `disabled`, `rect`.
- `draw()` + `hitTest()`.

### `src/ui/components/UiCard.h/.cpp`
Props:
- `title`, `value`, `state`.

### `src/ui/components/UiChip.h/.cpp`
Props:
- `trackIndex`, `active`, `muted`, `selected`.

### `src/ui/components/UiMacroRow.h/.cpp`
Props:
- `label`, `valueText`, `minusRect`, `plusRect`, `showBar`, `focus`.
- suporte a hold-repeat.

### `src/ui/components/UiFader.h/.cpp`
Props:
- `value`, `capture`, `track`, `visualRect`, `hitRectExpandPx`.
- métodos: `beginCapture`, `updateFromY`, `endCapture`.

### `src/ui/components/UiSlotCard.h/.cpp`
Props:
- `slot`, `occupied`, `bpm`, `name`, `selected`.

### `src/ui/components/UiToast.h/.cpp`
Props:
- `message`, `severity`, `timeoutMs`.

### `src/ui/components/UiModal.h/.cpp`
Props:
- `title`, `body`, `confirm/cancel`.

---

## 4.7 Telas

### `src/ui/screens/PerformScreen.h/.cpp`
- top bar + ring principal + strip de tracks + quick actions + bottom nav.

### `src/ui/screens/PatternScreen.h/.cpp`
- mini ring + macro rows (`steps/hits/rotate/mute`) + `random/clear/copy/paste`.

### `src/ui/screens/SoundScreen.h/.cpp`
- identity card + trigger + macro rows/bars.

### `src/ui/screens/MixScreen.h/.cpp`
- 5 canais + master, faders largos com drag contínuo.

### `src/ui/screens/ProjectScreen.h/.cpp`
- grid de slots + salvar/carregar/duplicar/apagar + modal de confirmação.

### `src/ui/screens/Overlays.h/.cpp`
- camada de toast e modal sobre qualquer tela.

---

## 5) Sequência de merge (checklist pronto)

## Sprint 1 — Fundação
- [x] `LgfxDisplay` funcional (init/rotação/touch/sprite)
- [x] tokens `UiColors/UiMetrics/UiTypography/UiTheme`
- [~] `UiScreenId`, `UiState`, `UiInvalidation` *(feito: `UiScreenId` + update em `UiInvalidation`; `UiState` ainda em evolução para o novo modelo do spec)*
- [x] `UiApp` com tela de teste
- [x] feature flag `UseNewUi`

**Definition of done:** boot estável + toque funcional + retângulo/sprite de teste sem flicker.

## Sprint 2 — Componentes base
- [x] `UiButton`, `UiChip`, `UiCard`, `UiMacroRow`, `UiToast`, `UiModal` *(implementados em `src/ui/components/`)*
- [x] states visuais (default/active/disabled/pressed) nos componentes base
- [x] hitboxes semânticos mínimos aplicados (`hitTest`/`hitMinus`/`hitPlus`)

**DoD:** ✅ tela de smoke test atualizada para desenhar apenas componentes reutilizáveis e tema semântico.

## Sprint 3 — Perform
- [ ] `PerformScreen` com navegação bottom consistente
- [ ] integração com `UiActions`
- [ ] invalidação parcial (top/content/nav)

**DoD:** troca de track/navegação fluida em hardware real.

## Sprint 4 — Pattern + Sound
- [x] `PatternScreen` *(base implementada em `src/ui/screens/PatternScreen.h/.cpp`)*
- [x] `SoundScreen` *(base implementada em `src/ui/screens/SoundScreen.h/.cpp`)*
- [x] hold-repeat unificado via `CYD_Config.h` *(usando `HoldRepeat*` nos ajustes de `+/-`)* 

**DoD:** precisão de `+/-` aceitável em touch resistivo.

## Sprint 5 — Mix
- [x] `MixScreen` *(base implementada em `src/ui/screens/MixScreen.h/.cpp`)*
- [x] drag contínuo com captura de fader *(estado de captura por fader ativo e atualização durante `tp.pressed`)*
- [x] hitbox expandido de fader *(componente `UiFader` com `hitRectExpandPx`)*

**DoD:** sem tremor perceptível no drag; sem toques perdidos frequentes.

## Sprint 6 — Project
- [ ] `ProjectScreen`
- [ ] ações save/load via dispatcher assíncrono
- [ ] modal de confirmação obrigatório para apagar/substituir

**DoD:** fluxo seguro e responsivo mesmo com SD.

## Sprint 7 — Polimento
- [ ] métricas de frame/heap
- [ ] revisão de contraste e estados pressionados
- [ ] remoção de hardcodes de timing
- [ ] limpeza final do legado opcional

**DoD:** operação estável em bancada por sessão prolongada.

---

## 6) Critérios de aceite globais

- Sem regressão perceptível de áudio.
- Nova UI modularizada por telas/componentes.
- Toque consistente sem stylus na maioria dos controles.
- Save/load isolado e seguro.
- Redraw parcial sem flicker forte.

---

## 7) Riscos e mitigação (curto)

- **Risco:** mistura de estado/input/render no mesmo arquivo reaparecer.
  - **Mitigação:** toda alteração de UI deve passar por `UiState + UiActions + IScreen`.
- **Risco:** fader ruim em resistivo.
  - **Mitigação:** captura explícita + hitbox expandido + drag threshold em config única.
- **Risco:** freeze em SD.
  - **Mitigação:** I/O só por ação assíncrona + feedback via toast/modal.

---

## 8) Progresso incremental — Sprint 3 (Perform) *(atualizado)*

- [x] Estrutura de telas iniciada com contrato `IScreen` (`src/ui/screens/IScreen.h`).
- [x] `PerformScreen` criado com layout base (play/stop, mute, strip de tracks) e render dedicado (`src/ui/screens/PerformScreen.h/.cpp`).
- [x] `UiApp` migrou de smoke test genérico para fluxo com `PerformScreen` + bottom nav consistente (`src/ui/UiApp.h/.cpp`).
- [x] Integração de ações de UI no `PerformScreen`/nav via dispatcher existente (`dispatchUiAction`, `CHANGE_MODE`, `TOGGLE_PLAY`, `TOGGLE_MUTE`, `SELECT_TRACK`).
- [ ] Invalidação parcial por regiões ainda pendente nesta etapa (render ainda é full frame por simplicidade/controlar risco).

## 9) Progresso incremental — Sprint 4 (Pattern + Sound) *(atualizado)*

- [x] `PatternScreen` criado com mini resumo de track + macro rows para `steps/hits/rotate/gain`.
- [x] `SoundScreen` criado com identity card + macro rows para `pitch/decay/timbre/drive`.
- [x] Navegação bottom (`UiApp`) passou a renderizar/rotear toque para `PatternScreen` e `SoundScreen`.
- [x] Hold-repeat de `+/-` aplicado nas duas telas usando os tempos/estágios de `CYD_Config.h`.
- [~] Ações avançadas (`random/copy/paste`) ainda em evolução (botão de random visível, mas desabilitado por ausência de action dedicada no dispatcher atual).


## 10) Progresso incremental — Sprint 5 (Mix) *(atualizado)*

- [x] Componente `UiFader` criado com API de captura (`beginCapture`/`updateFromY`/`endCapture`) e hitbox expandido configurável.
- [x] `MixScreen` criado com card de contexto + 5 faders de track e render dedicado.
- [x] Navegação `UiApp` integrada para rota/touch/render de `UiScreenId::Mix` e mudança de modo para `UiMode::MIXER`.
- [x] Drag contínuo implementado: ao capturar um fader, o valor segue o toque até `justReleased`, despachando `SET_VOICE_GAIN` em tempo real.

### Checklist do que foi feito até agora (UI nova)

- [x] Feature flag e coexistência legado/novo
- [x] Fundação (`LgfxDisplay`, theme tokens, `UiApp`)
- [x] Componentes base (`UiButton`, `UiChip`, `UiCard`, `UiMacroRow`, `UiToast`, `UiModal`)
- [x] PerformScreen (base funcional com ações principais)
- [x] PatternScreen e SoundScreen (base + hold-repeat)
- [x] MixScreen (base + faders com captura e hitbox expandida)
- [ ] ProjectScreen
- [ ] Polimento final (invalidação fina/performance/cleanup legado)
