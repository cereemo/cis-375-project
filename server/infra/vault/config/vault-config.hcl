listener "tcp" {
  address = "0.0.0.0:8200"
  tls_disable = "true"
}

ui = true

api_addr = "http://vault:8200"
cluster_addr = "http://vault:8201"

storage "raft" {
  path = "/vault/data"
  node_id = "vault-dev-1"
}

disable_mlock = false