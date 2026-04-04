# План разработки Frabgine Engine MVP

## Этап 0: Исследование и подготовка ✓ (ЗАВЕРШЕН)

### Выполненные задачи:
- [x] Анализ архитектурной философии wz4
- [x] Проектирование модульной структуры
- [x] Создание базовой структуры проекта
- [x] Реализация ядра (Core):
  - [x] Математические типы (Vector, Matrix, Quaternion)
  - [x] Умные указатели (UniquePtr, SharedPtr, RefPtr)
  - [x] Пулы памяти и аллокаторы
  - [x] Сериализация (JSON, Binary)
  - [x] Утилиты (Logger, Timer, TaskScheduler, FileUtils)
- [x] Создание VulkanRenderer (базовая реализация)
- [x] Система сцены и компонентов (SceneGraph)
- [x] Узловой граф (NodeGraph) для редактора
- [x] Настройка CMake сборки
- [x] Документация (README, архитектурные принципы)

**Итого файлов**: 23 исходных файла (.hpp/.cpp)

---

## Этап 1: Завершение графического ядра (СЛЕДУЮЩИЙ)

### 1.1 Ресурсы Vulkan
- [ ] Buffer.hpp/cpp - Буферы вершин и индексов
- [ ] Texture.hpp/cpp - Управление текстурами
- [ ] Shader.hpp/cpp - Компиляция и управление шейдерами
- [ ] Pipeline.hpp/cpp - Графические пайплайны
- [ ] DescriptorSet.hpp/cpp - Дескрипторы ресурсов

### 1.2 Менеджер ресурсов
- [ ] ResourceManager.hpp/cpp
  - Асинхронная загрузка
  - Кэширование
  - Управление временем жизни

### 1.3 Интеграция с NodeGraph
- [ ] MaterialCompiler.hpp/cpp - Компиляция графа материалов в шейдеры
- [ ] ParticleSystem.hpp/cpp - Система частиц на основе узлов

---

## Этап 2: Системы движка

### 2.1 Физика (PhysX интеграция)
- [ ] PhysicsWorld.hpp/cpp
- [ ] RigidBodyComponent.hpp
- [ ] ColliderComponent.hpp
- [ ] CharacterController.hpp

### 2.2 Аудио (OpenAL интеграция)
- [ ] AudioDevice.hpp/cpp
- [ ] AudioSourceComponent.hpp
- [ ] AudioListenerComponent.hpp

### 2.3 Анимация
- [ ] Skeleton.hpp/cpp
- [ ] AnimationClip.hpp/cpp
- [ ] AnimatorComponent.hpp
- [ ] BlendTree.hpp/cpp (на основе NodeGraph)

---

## Этап 3: Редактор на Qt 6

### 3.1 Базовый интерфейс
- [ ] MainWindow.hpp/cpp
- [ ] SceneHierarchyWidget.hpp/cpp
- [ ] PropertiesWidget.hpp/cpp
- [ ] ViewportWidget.hpp/cpp (Vulkan в Qt)

### 3.2 Узловой редактор
- [ ] NodeEditorWidget.hpp/cpp
- [ ] NodeGraphicsItem.hpp/cpp
- [ ] ConnectionGraphicsItem.hpp/cpp
- [ ] NodePaletteWidget.hpp/cpp

### 3.3 Инструменты
- [ ] GizmoTools.hpp (Move, Rotate, Scale)
- [ ] SelectionManager.hpp
- [ ] UndoRedoSystem.hpp

---

## Этап 4: Скриптинг и игровой процесс

### 4.1 Lua интеграция
- [ ] LuaState.hpp/cpp
- [ ] ScriptComponent.hpp
- [ ] LuaBindings.hpp (для API движка)

### 4.2 Игровые системы
- [ ] InputSystem.hpp/cpp
- [ ] UISystem.hpp/cpp (Dear ImGui или Qt)
- [ ] GameStateManager.hpp

---

## Этап 5: Оптимизация и инструменты

### 5.1 Профилирование
- [ ] Profiler.hpp/cpp
- [ ] FrameDebugger.hpp
- [ ] MemoryTracker.hpp

### 5.2 Инструменты разработчика
- [ ] Console.hpp/cpp
- [ ] DebugRenderer.hpp
- [ ] HotReloadSystem.hpp

### 5.3 CI/CD
- [ ] GitHub Actions workflow
- [ ] Автоматические тесты
- [ ] Сборка документации

---

## Критерии завершения MVP

1. ✅ **Ядро**: Математика, память, сериализация, утилиты
2. ⬜ **Рендеринг**: Vulkan с загрузкой моделей и текстур
3. ⬜ **Сцена**: Компонентная система с Transform, Camera, Mesh
4. ⬜ **Редактор**: Базовый UI с инспектором и иерархией
5. ⬜ **Узлы**: Граф материалов с компиляцией в шейдеры
6. ⬜ **Скрипты**: Lua интеграция для логики

---

## Технические метрики

- **Строк кода**: ~3000+ (ядро готово)
- **Файлов**: 23 (ядро готово)
- **Библиотек**: 3 (core, graphics, engine)
- **Зависимостей**: Vulkan, nlohmann_json, Qt6 (опционально)

---

## Риски и проблемы

1. **Vulkan сложность**: Требует детальной проработки каждого компонента
2. **Qt интеграция**: Vulkan + Qt требует careful настройки surface
3. **Производительность**: NodeGraph execution needs optimization
4. **Сериализация std::any**: Требует visitor pattern для полной поддержки

---

*Документ последний раз обновлен: 2024*
