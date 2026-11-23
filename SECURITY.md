# Revisão de exposição — 23/11/2026

Revisão da árvore versionada e de todo o histórico alcançável das branches e tags disponíveis no remoto antes de habilitar leitura pública.

## Resultado

- Gitleaks 8.30.1: nenhuma credencial detectada na varredura Git com `--log-opts=--all` e saída com segredos ocultados.
- Inspeção complementar de 300 blobs únicos, incluindo 24 binários: nenhuma correspondência para chaves privadas, tokens de provedores, URLs com usuário/senha ou atribuições explícitas de senhas/chaves.
- Metadados de commits: somente endereço GitHub noreply do autor; nenhum e-mail pessoal do proprietário identificado. Créditos e contatos públicos dos autores da biblioteca USB permanecem preservados.
- GitHub: não havia releases/anexos, issues, artefatos ou caches de Actions na revisão. Metadados consultados também passaram pela varredura.
- `config.json`, `fleet.json`, arquivos de ambiente, chaves e logs locais ficam excluídos do versionamento. Diagnósticos locais não foram publicados.

## Identificadores presentes

O repositório não é anônimo. Nome/usuário público do proprietário aparece na documentação e nos commits. Alguns binários de backup contêm caminhos de compilação com o nome de usuário Linux; há um endereço BLE legado em uma versão antiga e nos respectivos registros. Esses identificadores não são senhas, tokens ou chaves de acesso remoto e não autorizam acesso ao computador ou a serviços. Os backups validados e seus checksums foram preservados, sem modificar os binários.

## Limites

A revisão não é uma prova de ausência absoluta de segredos. Verificou padrões conhecidos e identificadores comuns, sem usar credenciais encontradas nem testar acesso a contas. Novos commits precisam de nova revisão. A configuração pública permite leitura, clone e fork; não concede permissão de escrita ao repositório original.
