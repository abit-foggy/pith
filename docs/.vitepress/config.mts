import { defineConfig } from 'vitepress'

// Infer repo name for base path; adjust if using custom domain
const base = process.env.GITHUB_REPOSITORY
  ? `/${process.env.GITHUB_REPOSITORY.split('/')[1]}/`
  : '/'

export default defineConfig({
  title: 'Pith Docs',
  description: 'Documentation for Pith',
  base: base,
  themeConfig: {
    search: {
      provider: 'local',
    },
    nav: [
      { text: 'Home', link: '/' },
      { text: 'Guide', link: '/getting-started' },
      { text: 'Language', link: '/language' },
      { text: 'FFI', link: '/ffi' },
      { text: 'Runtime', link: '/runtime' },
    ],
    sidebar: [
      {
        text: 'Getting Started',
        items: [
          { text: 'Introduction', link: '/getting-started' },
        ],
      },
      {
        text: 'Reference',
        items: [
          { text: 'Language Syntax', link: '/language' },
          { text: 'CLI Commands', link: '/cli' },
          { text: 'Runtime & Memory', link: '/runtime' },
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
