import { defineConfig } from 'vitepress'

const base = process.env.GITHUB_REPOSITORY
  ? `/${process.env.GITHUB_REPOSITORY.split('/')[1]}/`
  : '/'

export default defineConfig({
  title: 'Pith',
  description: 'A dead-simple, bracketless systems-scripting language compiled via QBE',
  base: base,
  themeConfig: {
    search: {
      provider: 'local',
    },
    nav: [
      { text: 'Home', link: '/' },
      { text: 'Get Started', link: '/getting-started/installation' },
      { text: 'Language', link: '/language' },
      { text: 'CLI', link: '/cli/run' },
      { text: 'FFI', link: '/ffi' },
    ],
    sidebar: [
      {
        text: 'Getting Started',
        items: [
          { text: 'Installation', link: '/getting-started/installation' },
          { text: 'Your First Script', link: '/getting-started/first-script' },
          { text: 'Setting Up a Project', link: '/getting-started/project-setup' },
        ],
      },
      {
        text: 'Language Reference',
        items: [
          { text: 'Syntax & Semantics', link: '/language' },
          { text: 'Namespaces', link: '/namespaces' },
          { text: 'Runtime & Memory Model', link: '/runtime' },
        ],
      },
      {
        text: 'Configuration',
        items: [
          { text: 'pith.toml & pith.lock', link: '/config' },
        ],
      },
      {
        text: 'CLI Reference',
        items: [
          { text: 'pith run', link: '/cli/run' },
          { text: 'pith build', link: '/cli/build' },
          { text: 'pith decompile', link: '/cli/decompile' },
          { text: 'pith pkg', link: '/cli/pkg' },
          { text: 'pith engine', link: '/cli/engine' },
          { text: 'Custom Tasks', link: '/cli/tasks' },
        ],
      },
      {
        text: 'Interop',
        items: [
          { text: 'C Imports (FFI)', link: '/ffi' },
          { text: 'Embeddable C ABI', link: '/embed' },
        ],
      },
    ],
    socialLinks: [
      { icon: 'github', link: 'https://github.com/abit-foggy/pith' },
    ],
  },
})
