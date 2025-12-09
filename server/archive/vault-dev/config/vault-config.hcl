listener "tcp" {
  address     = "0.0.0.0:8200"
  tls_disable = "true" # IMPORTANT: Disable for DEV, MUST be enabled for PROD
}

ui = true
api_addr = "http://127.0.0.1:8200"
cluster_addr = "http://127.0.0.1:8201"

storage "raft" {
  path    = "/vault/data"
  node_id = "vault-dev-1"
}

disable_mlock = false